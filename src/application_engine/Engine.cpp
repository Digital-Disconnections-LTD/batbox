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
    return model->microcompactMessages(messages,context,source);
  };
  require(inputs_.checkpoint,"local checkpoint");
  require(inputs_.logError,"local error log");
  if(inputs_.flightId.empty())throw std::invalid_argument("Application session ID required");
  services_=std::make_shared<query::AsyncEngineServices>();
  const auto flight=inputs_.flightId;
  services_->config.getFlightId=inputs_.flightIdProvider?
    inputs_.flightIdProvider:std::function<std::string()>([flight]{return flight;});
  const auto features=inputs_.features;
  services_->config.statsigFeatureGate=[features](const std::string& name){
    return features.contains(name);
  };
  bindSetup();bindModel();bindCompletion();bindGadgets();
  require(inputs_.startMemoryPrefetch,"memory prefetch provider");
  require(inputs_.postSamplingHooks,"post-sampling hook provider");
  services_->startRelevantMemoryPrefetch=inputs_.startMemoryPrefetch;
  services_->executePostSamplingHooks=inputs_.postSamplingHooks;
  services_->refreshUserContextAfterCompact=inputs_.refreshUserContextAfterCompact;
  query::bindIndigenousAbortRemaining(inputs_.owner,*services_);
}
void Engine::bindSetup(){
  auto& async=services_->setup;auto& sync=async.sync;
  async.queryCheckpoint=inputs_.checkpoint;
  async.headlessProfilerCheckpoint=inputs_.checkpoint;
  const auto features=inputs_.features;
  sync.feature=[features](const std::string& name){return features.contains(name);};
  sync.uuid=inputs_.messages->randomUUID;
  messages::BoundaryServices boundary;
  boundary.randomUUID=inputs_.messages->randomUUID;
  boundary.nowISO=inputs_.messages->nowISO;
  boundary.feature=sync.feature;
  sync.getMessagesAfterCompactBoundary=[boundary](const Json& messages)mutable{
    return messages::getMessagesAfterCompactBoundary(messages,false,boundary);
  };
  require(inputs_.recordContentReplacement,"content replacement transcript writer");
  sync.recordContentReplacement=inputs_.recordContentReplacement;
  async.applyGadgetResultBudget=[owner=&inputs_.owner,budget=inputs_.budget](
      const Json& messages,const auto& state,const auto& persist,const auto& unlimited){
    event_loop::Deferred<Json> result(*owner);
    // Without replacement state the budget returns its input unchanged; skip
    // copying the conversation into message references and back.
    if(!state){result.resolve(messages);return result;}
    auto pending=gadget_results::applyGadgetResultBudget(*owner,budget,
      gadget_results::makeMessageArray(messages),state,persist,
      std::unordered_set<std::string>(unlimited.begin(),unlimited.end()));
    pending.then([result](const gadget_results::MessageArray* value,
        const std::exception_ptr& error){
      if(error)result.reject(error);
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
  sync.isAutoCompactEnabled=[policy,fallback=inputs_.config.model]{
    return batbox::isAutoCompactEnabled(policy(fallback));
  };
  sync.isReactiveCompactEnabled=[]{return false;};
  sync.isContextCollapseEnabled=[]{return false;};
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
  sync.feature=services_->setup.sync.feature;
  sync.getAppState=inputs_.getAppState;
  require(inputs_.prependUserContext,"user context");
  sync.prependUserContext=inputs_.prependUserContext;
  sync.normalizeMessagesForAPI=inputs_.normalize;
  sync.createUserMessage=[messages=inputs_.messages](const Json& args){
    return messages::createUserMessage(args,*messages);
  };
  sync.createAssistantAPIErrorMessage=services_->setup.sync.createAssistantAPIErrorMessage;
  sync.addNotification=inputs_.addNotification;
  // Actual API and decoder errors remain in the model/retry pipeline. Media
  // resize recovery is enabled only when its application provider is bound.
  sync.isImageSizeOrResizeError=[](const std::exception&){return false;};
  async.callModel=[model=inputs_.model](const query::ModelRequest& request,
      const std::function<void()>& fallback){return model->makeCursor(request,fallback);};
  async.queryCheckpoint=inputs_.checkpoint;
  async.logError=inputs_.logError;async.logInternalError=inputs_.logError;
}
void Engine::bindCompletion(){
  auto completion=inputs_.completion;
  completion.sync.feature=services_->setup.sync.feature;
  completion.sync.createUserMessage=services_->model.sync.createUserMessage;
  completion.sync.finalContextTokensFromLastResponse=inputs_.finalContextTokens;
  completion.sync.buildPostCompactMessages=compaction::buildPostCompactMessages;
  completion.sync.makeIndigenousStashSafeParams=bats::makeStashSafeSnapshot;
  completion.reactiveCompactAvailable=[]{return false;};
  completion.contextCollapseAvailable=[]{return false;};
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
  gadgets.sync.feature=services_->setup.sync.feature;
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
