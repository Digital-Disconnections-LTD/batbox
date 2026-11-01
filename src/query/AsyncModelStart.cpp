#include "query/AsyncModelInternal.hpp"
#include "retry/Retry.hpp"

namespace batbox::query {
AsyncModelPassIterator makeAsyncModelPassIterator(event_loop::Loop& loop,
  Params params,State state,std::shared_ptr<PreparedIteration> prepared,
  QueryConfig config,std::optional<double> missionBudgetRemaining,
  AsyncModelServices services){
  if(!loop.isOwnerThread())
    throw std::logic_error("Async model pass must start on the owner loop");
  if(!prepared)throw std::invalid_argument("Async model pass requires prepared iteration");
  return AsyncModelPassIterator(std::make_shared<async_model::Run>(loop,
    std::move(params),std::move(state),std::move(prepared),std::move(config),
    missionBudgetRemaining,std::move(services)));
}
event_loop::Deferred<AsyncModelStep> AsyncModelPassIterator::next(){return run_->next();}
} // namespace batbox::query

namespace batbox::query::async_model {
namespace {
bool pendingMcp(const Json& appState){
  const Json& clients=field(field(appState,"mcp"),"clients");
  if(clients.is_array())for(const Json& client:clients)
    if(field(client,"type")=="pending")return true;
  return false;
}
ModelRequest makeRequest(const Run& run){
  const auto& p=run.params;
  const auto& prepared=*run.prepared;
  const auto& sync=run.services.sync;
  need(sync.prependUserContext,"prependUserContext");
  ModelRequest request;
  request.messages=sync.prependUserContext(prepared.messagesForQuery,p.userContext);
  request.systemPoetry=prepared.fullSystemPoetry;
  const Json& options=field(prepared.gadgetUseContext.data,"options");
  request.thinkingConfig=field(options,"thinkingConfig");
  request.gadgets=prepared.gadgetUseContext.gadgets;
  request.signal=prepared.gadgetUseContext.stop;
  // The model service copies this callback many times; share one context
  // without the conversation, which the app-state lookup does not read.
  const auto context=std::make_shared<const execution::Context>(
    execution::contextWithoutConversation(prepared.gadgetUseContext));
  const auto getAppState=sync.getAppState;
  request.getGadgetPermissionContext=[context,getAppState]{
    need(getAppState,"getAppState");
    return field(execution::effectiveAppState(*context,getAppState(*context)),
      "toolPermissionContext");
  };
  request.addNotification=sync.addNotification;
  Json modelOptions={{"model",prepared.currentModel},
    {"isNonInteractiveSession",field(options,"isNonInteractiveSession")},
    {"querySource",p.querySource},
    {"agents",field(field(options,"agentDefinitions"),"activeAgents")},
    {"allowedAgentTypes",field(field(options,"agentDefinitions"),"allowedAgentTypes")},
    {"hasAppendSystemPrompt",value::truthy(field(options,"appendSystemPrompt"))},
    {"maxOutputTokensOverride",run.state.maxOutputTokensOverride?
      Json(*run.state.maxOutputTokensOverride):Json()},
    {"mcpTools",field(field(prepared.appState,"mcp"),"tools")},
    {"hasPendingMcpServers",pendingMcp(prepared.appState)},
    {"queryTracking",prepared.queryTracking},
    {"effortValue",field(prepared.appState,"effortValue")},
    {"advisorModel",field(prepared.appState,"advisorModel")},
    {"skipCacheWrite",p.skipStashWrite},
    {"agentId",field(prepared.gadgetUseContext.data,"agentId")}};
  if(p.fallbackModel)modelOptions["fallbackModel"]=*p.fallbackModel;
  if(run.config.fastModeEnabled)modelOptions["fastMode"]=field(prepared.appState,"fastMode");
  if(p.missionBudgetTotal){
    modelOptions["taskBudget"]={{"total",*p.missionBudgetTotal}};
    if(run.remaining)modelOptions["taskBudget"]["remaining"]=*run.remaining;
  }
  request.options=std::move(modelOptions);
  return request;
}
}
event_loop::Deferred<AsyncModelStep> Run::next(){
  if(!loop.isOwnerThread())throw std::logic_error("Async model next() requires loop owner");
  event_loop::Deferred<AsyncModelStep> request(loop);
  requests.push_back(request);
  pump();
  return request;
}
void Run::emit(const model::MessageRef& message,Stage after,
    model::MessageRef tombstoneTarget){
  stage=after;pausedAfterYield=true;
  auto request=requests.front();requests.pop_front();
  request.resolve(AsyncModelStep{false,message,{},std::move(tombstoneTarget)});
  if(!requests.empty()){
    auto self=shared_from_this();loop.queueMicrotask([self]{self->pump();});
  }
}
void Run::finish(){
  syncAssistantValues();stage=Stage::Done;
  auto value=std::make_shared<ModelPassResult>(std::move(result));
  auto request=requests.front();requests.pop_front();
  request.resolve(AsyncModelStep{true,{},std::move(value)});
  while(!requests.empty()){
    auto queued=requests.front();requests.pop_front();
    queued.resolve(AsyncModelStep{true,{},{}});
  }
}
void Run::fail(std::exception_ptr error){
  failure=std::move(error);stage=Stage::Done;
  if(!requests.empty()){
    auto request=requests.front();requests.pop_front();request.reject(failure);
  }
  while(!requests.empty()){
    auto queued=requests.front();requests.pop_front();queued.resolve(AsyncModelStep{true,{},{}});
  }
}
void Run::pump(){
  if(requests.empty())return;
  try{
    while(!requests.empty()){
      switch(stage){
        case Stage::Start:
          need(services.queryCheckpoint,"queryCheckpoint");
          services.queryCheckpoint("query_api_loop_start");
          stage=Stage::StartAttempt;break;
        case Stage::StartAttempt:startAttempt();break;
        case Stage::AwaitMessage:case Stage::InjectAwait:return;
        case Stage::HandleMessage:handleMessage();break;
        case Stage::AfterTombstones:afterTombstones();break;
        case Stage::AfterAssistantYield:afterAssistantYield();break;
        case Stage::EmitGadgetResult:processGadgetResults();break;
        case Stage::AfterGadgetResultYield:afterGadgetResultYield();break;
        case Stage::AfterStream:afterStream();break;
        case Stage::EmitMissing:emitMissing();break;
        case Stage::AfterMissing:afterMissing();break;
        case Stage::EmitFallbackSystem:emitFallbackSystem();break;
        case Stage::AfterFallbackSystem:afterFallbackSystem();break;
        case Stage::EmitFailureMessage:emitFailureMessage();break;
        case Stage::AfterFailureMessage:afterFailureMessage();break;
        case Stage::AfterStashedBoundary:finish();break;
        case Stage::Done:{
          auto request=requests.front();requests.pop_front();
          request.resolve(AsyncModelStep{true,{},{}});break;
        }
      }
      if(pausedAfterYield){pausedAfterYield=false;return;}
    }
  }catch(...){
    const auto error=std::current_exception();
    if(stage==Stage::Start||stage==Stage::EmitFailureMessage||
       stage==Stage::AfterFailureMessage||
       (handlingError&&!fallbackNames)){
      fail(error);return;
    }
    try{onStreamError(error,!handlingError);pump();}
    catch(...){fail(std::current_exception());}
  }
}
void Run::startAttempt(){
  handlingError=false;
  need(services.callModel,"callModel");
  services.queryCheckpoint("query_api_streaming_start");
  streamingFallbackOccurred.store(false);
  std::weak_ptr<Run> owner=shared_from_this();
  cursor=services.callModel(makeRequest(*this),[owner]{
    if(const auto run=owner.lock())
      run->streamingFallbackOccurred.store(true);});
  need(cursor.next,"callModel cursor.next");
  requestMessage();
}
void Run::requestMessage(){
  auto pending=cursor.next();
  if(!loop.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Async model stream belongs to another event loop");
  stage=Stage::AwaitMessage;
  auto self=shared_from_this();
  pending.then([self](const std::optional<model::MessageRef>* value,
      const std::exception_ptr& error){
    if(error){
      try{self->onStreamError(error);self->pump();}
      catch(...){self->fail(std::current_exception());}
      return;
    }
    try{self->onMessage(*value);self->pump();}
    catch(...){
      try{self->onStreamError(std::current_exception());self->pump();}
      catch(...){self->fail(std::current_exception());}
    }
  });
}
void Run::onMessage(const std::optional<model::MessageRef>& message){
  if(!message){stage=Stage::AfterStream;return;}
  currentMessage=*message;
  stage=Stage::HandleMessage;
}
void Run::syncAssistantValues(){
  for(std::size_t i=0;i<result.assistantRefs.size()&&i<result.assistantMessages.size();++i)
    result.assistantMessages[i]=*result.assistantRefs[i];
}
} // namespace batbox::query::async_model
