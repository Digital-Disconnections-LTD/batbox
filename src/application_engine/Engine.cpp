#include "application_engine/Engine.hpp"
#include "application_engine/AutoPolicy.hpp"
#include "application/Async.hpp"
#include "compaction/Compaction.hpp"
#include "query/IndigenousGadgetCursor.hpp"
#include "services/compact/autoCompact.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::application_engine {
namespace {
template<class T> void require(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Application engine requires ")+name);
}
}
std::shared_ptr<Engine> Engine::create(Inputs inputs){
  auto engine=std::shared_ptr<Engine>(new Engine(std::move(inputs)));
  engine->initialize();return engine;
}
std::shared_ptr<Engine> Engine::forBat(const bats::BatContext& context,
    std::function<std::future<Json>(const Json&)> canUseGadget)const{
  if(!inputs_.bindBatPermission)
    throw std::invalid_argument("Agent permission resolver binding required");
  if(!canUseGadget)
    throw std::invalid_argument("Agent permission callback required");
  auto inputs=inputs_;
  inputs.execution=std::make_shared<execution::Services>(*inputs_.execution);
  inputs.execution->canUseGadget=canUseGadget;
  const auto owner=inputs.owner.dispatcher();
  inputs.getAppState=[owner,get=context.getAppState](const execution::Context&){
    if(!get)throw std::invalid_argument("Agent app-state callback required");
    return application::onOwnerAndWait<Json>(owner,get);
  };
  inputs.execution->getAppState=inputs.getAppState;
  inputs.bindBatPermission(*inputs.execution,inputs.getAppState,canUseGadget);
  inputs.stopHooks.getAppState=inputs.getAppState;
  // A projected indigenous BatContext can omit the live reason callback even
  // when it carries an abortReason value. Keep the async abort path
  // bound for that child while preserving the interrupt distinction.
  const auto currentAbortReason=context.currentAbortReason?
    context.currentAbortReason:std::function<std::string()>(
      [reason=context.query.abortReason]{return reason;});
  inputs.execution->currentAbortReason=currentAbortReason;
  inputs.gadgetTurn.sync.currentAbortReason=currentAbortReason;
  inputs.execution->makeBatContext=[context](const execution::Context& current,
      const Json& callData){
    auto live=std::make_shared<bats::BatContext>(context);
    live->query=current;live->query.data=callData;return live;
  };
  return create(std::move(inputs));
}
void Engine::initialize(){
  require(inputs_.model,"model");require(inputs_.messages,"messages");
  require(inputs_.execution,"execution");require(inputs_.getAppState,"getAppState");
  const auto model=inputs_.model;
  if(!inputs_.normalize)inputs_.normalize=[model](const Json& messages,const gadgets::Gadgets& gadgets){
    return model->normalizeMessagesForAPI(messages,gadgets);
  };
  if(!inputs_.appendSystemContext)inputs_.appendSystemContext=[model](const Json& poetry,const Json& context){
    return model->appendSystemContext(poetry,context);
  };
  if(!inputs_.prependUserContext)inputs_.prependUserContext=[model](const Json& messages,const Json& context){
    return model->prependUserContext(messages,context);
  };
  if(!inputs_.microcompact)inputs_.microcompact=[model](const Json& messages,
      const execution::Context& context,const std::string& source){
    return model->microcompactForQuery(messages,context,source);
  };
  require(inputs_.checkpoint,"local checkpoint");
  require(inputs_.logError,"local error log");
  services_=std::make_shared<query::AsyncEngineServices>();
  bindSetup();bindModel();bindCompletion();bindGadgets();
  require(inputs_.startMemoryPrefetch,"memory prefetch provider");
  services_->startRelevantMemoryPrefetch=inputs_.startMemoryPrefetch;
  services_->executePostSamplingHooks=inputs_.postSamplingHooks;
  services_->refreshUserContextAfterCompact=inputs_.refreshUserContextAfterCompact;
}
void Engine::bindSetup(){
  auto& async=services_->setup;auto& sync=async.sync;
  async.queryCheckpoint=inputs_.checkpoint;
  async.headlessProfilerCheckpoint=inputs_.checkpoint;
  sync.uuid=inputs_.messages->randomUUID;
  sync.getMessagesAfterCompactBoundary=[](Json messages){
    return messages::getMessagesAfterCompactBoundary(std::move(messages));
  };
  require(inputs_.recordContentReplacement,"content replacement transcript writer");
  sync.recordContentReplacement=inputs_.recordContentReplacement;
  async.applyGadgetResultBudget=[owner=&inputs_.owner,budget=inputs_.budget](
      const Json& messages,const auto& state,const auto& persist,const auto& unlimited){
    event_loop::Deferred<Json> result(*owner);
    // Without replacement state the budget returns its input unchanged; skip
    // copying the conversation into message references and back.
    if(!state){result.resolve(query::unchangedMessages());return result;}
    auto input=gadget_results::makeMessageArray(messages);
    auto pending=gadget_results::applyGadgetResultBudget(*owner,budget,input,state,persist,
      std::unordered_set<std::string>(unlimited.begin(),unlimited.end()));
    // The budget hands back the array it was given when it replaced nothing.
    pending.then([result,input](const gadget_results::MessageArray* value,
        const std::exception_ptr& error){
      if(error)result.reject(error);
      else if(*value==input)result.resolve(query::unchangedMessages());
      else result.resolve(gadget_results::snapshotMessages(*value));
    });return result;
  };
  require(inputs_.microcompact,"microcompact");
  require(inputs_.appendSystemContext,"system context");
  require(inputs_.autocompact,"autocompact");
  require(inputs_.autocompactIndigenous,"native autocompact");
  async.microcompact=inputs_.microcompact;
  sync.appendSystemContext=inputs_.appendSystemContext;
  async.autocompact=inputs_.autocompact;
  async.autocompactIndigenous=inputs_.autocompactIndigenous;
  async.autocompactWouldRun=inputs_.autocompactWouldRun;
  sync.makeIndigenousStashSafeParams=bats::makeStashSafeSnapshot;
  sync.buildPostCompactMessages=compaction::buildPostCompactMessages;
  sync.getAppState=inputs_.getAppState;
  sync.finalContextTokensFromLastResponse=inputs_.finalContextTokens;
  sync.doesMostRecentAssistantMessageExceed200k=[count=inputs_.finalContextTokens](const Json& messages){
    return count(messages)>200000;
  };
  sync.getRuntimeMainLoopModel=[fallback=inputs_.config.model](const Json& args){
    const auto& chosen=value::property(args,"mainLoopModel");
    return chosen.is_string()?chosen.get<std::string>():fallback;
  };
  const auto policy=[config=inputs_.config,enabled=inputs_.autoCompactEnabled,
      model=inputs_.model,settings=inputs_.compactionSettings](const std::string& name){
    return autoCompactDependencies(config,enabled?enabled():true,
      name,model.get(),settings?settings():Json::object());
  };
  require(inputs_.tokenCount,"token estimator");
  require(inputs_.finalContextTokens,"final context token accounting");
  sync.tokenCountWithEstimation=inputs_.tokenCount;
  sync.calculateTokenWarningState=[policy,fallback=inputs_.config.model](
      double tokens,const Json& selected){
    const auto name=selected.is_string()?selected.get<std::string>():fallback;
    return calculateTokenWarningState(tokens,policy(name));
  };
  sync.createAssistantAPIErrorMessage=[messages=inputs_.messages](const Json& args){
    return messages::createAssistantAPIErrorMessage(args,*messages);
  };
  sync.poetryTooLongErrorMessage=compaction::ERROR_POETRY_TOO_LONG;
  sync.executionServices=inputs_.execution.get();
}
void Engine::bindModel(){
  auto& async=services_->model;auto& sync=async.sync;
  sync.getAppState=inputs_.getAppState;
  require(inputs_.prependUserContext,"user context");
  sync.prependUserContext=inputs_.prependUserContext;
  sync.createUserMessage=[messages=inputs_.messages](const Json& args){
    return messages::createUserMessage(args,*messages);
  };
  sync.createAssistantAPIErrorMessage=services_->setup.sync.createAssistantAPIErrorMessage;
  sync.addNotification=inputs_.addNotification;
  // Actual API and decoder errors remain in the model/retry pipeline. Media
  // resize recovery is enabled only when its application provider is bound.
  sync.isImageSizeOrResizeError=[](const std::exception&){return false;};
  async.callModel=[model=inputs_.model](query::ModelRequest request,
      const std::function<void()>& fallback){return model->makeCursor(std::move(request),fallback);};
  async.queryCheckpoint=inputs_.checkpoint;
  async.logError=inputs_.logError;async.logInternalError=inputs_.logError;
}
void Engine::bindCompletion(){
  auto completion=inputs_.completion;
  completion.sync.createUserMessage=services_->model.sync.createUserMessage;
  completion.handleStopHooks=[owner=&inputs_.owner,hooks=inputs_.stopHooks](
      const Json& messages,const Json& assistants,const query::Params& params,
      const execution::Context& context,std::optional<bool> active){
    return makeStopCursor(*owner,messages,assistants,params,context,active,hooks);
  };
  query::bindAsyncCompletion(inputs_.owner,*services_,std::move(completion));
}
void Engine::bindGadgets(){
  auto gadgets=inputs_.gadgetTurn;
  gadgets.queryCheckpoint=inputs_.checkpoint;
  gadgets.sync.executionServices=inputs_.execution.get();
  gadgets.sync.normalizeMessagesForAPI=inputs_.normalize;
  require(gadgets.sync.getCommandsByMaxPriority,"queued command selection");
  require(gadgets.sync.isSlashCommand,"queued command classification");
  require(gadgets.getAttachmentMessages,"attachment collection");
  query::bindIndigenousGadgetUpdates(inputs_.owner,gadgets);
  services_->gadgetTurn=gadgets.sync;
  query::bindAsyncGadgetTurn(inputs_.owner,*services_,std::move(gadgets));
}
} // namespace batbox::application_engine
