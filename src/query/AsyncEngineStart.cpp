#include "query/AsyncEngineInternal.hpp"

namespace batbox::query {
AsyncEngineIterator makeAsyncQueryLoop(event_loop::Loop& loop,Params params,
  std::vector<std::string>& consumedCommandUuids,AsyncEngineServices services){
  if(!loop.isOwnerThread())throw std::logic_error("Async query must start on loop owner");
  return AsyncEngineIterator(std::make_shared<async_engine::Run>(loop,
    std::move(params),consumedCommandUuids,std::move(services)));
}
} // namespace batbox::query

namespace batbox::query::async_engine {
event_loop::Deferred<AsyncEngineStep> Run::next(){
  if(!loop.isOwnerThread())throw std::logic_error("Async query next() requires loop owner");
  event_loop::Deferred<AsyncEngineStep> request(loop);
  requests.push_back(request);pump();return request;
}
void Run::emit(const model::MessageRef& event,Stage after,
    model::MessageRef tombstoneTarget){
  stage=after;pausedAfterYield=true;
  auto request=requests.front();requests.pop_front();
  request.resolve(AsyncEngineStep{false,event,{},std::move(tombstoneTarget)});
  if(!requests.empty()){
    auto self=shared_from_this();loop.queueMicrotask([self]{self->pump();});
  }
}
void Run::dispose(std::exception_ptr prior){
  if(disposalAttempted||!memoryPrefetch.dispose)return;
  disposalAttempted=true;
  try{memoryPrefetch.dispose();}
  catch(...){
    if(prior)throw QuerySuppressedError(std::current_exception(),prior);
    throw;
  }
}
void Run::finish(const Json& terminal){
  dispose();stage=Stage::Done;
  auto request=requests.front();requests.pop_front();
  request.resolve(AsyncEngineStep{true,{},terminal});
  while(!requests.empty()){
    auto queued=requests.front();requests.pop_front();
    queued.resolve(AsyncEngineStep{true,{},{}});
  }
}
void Run::fail(std::exception_ptr error){
  try{dispose(error);}catch(...){error=std::current_exception();}
  failure=std::move(error);stage=Stage::Done;
  if(!requests.empty()){
    auto request=requests.front();requests.pop_front();request.reject(failure);
  }
  while(!requests.empty()){
    auto queued=requests.front();requests.pop_front();
    queued.resolve(AsyncEngineStep{true,{},{}});
  }
}
void Run::pump(){
  if(requests.empty())return;
  try{
    while(!requests.empty()){
      switch(stage){
        case Stage::Start:start();break;
        case Stage::SetupStart:pullSetup();break;
        case Stage::SetupAwait:case Stage::ModelAwait:
        case Stage::AbortAwait:case Stage::AbortCleanupAwait:
        case Stage::SummaryAwait:case Stage::DecisionAwait:
        case Stage::ContextAwait:return;
        case Stage::ModelStart:pullModel();break;
        case Stage::AfterModel:afterModel();break;
        case Stage::AbortStart:startAbort();break;
        case Stage::AbortMissing:emitMissing();break;
        case Stage::AbortCleanup:startAbortCleanup();break;
        case Stage::AbortInterruption:emitAbortInterruption();break;
        case Stage::AbortTerminal:finish(Json{{"reason","aborted_streaming"}});break;
        case Stage::SummaryStart:startSummary();break;
        case Stage::DecisionStart:startDecision();break;
        case Stage::Done:{
          auto request=requests.front();requests.pop_front();
          request.resolve(AsyncEngineStep{true,{},{}});break;
        }
      }
      if(pausedAfterYield){pausedAfterYield=false;return;}
    }
  }catch(...){fail(std::current_exception());}
}
void Run::start(){
  need(services.setup.sync.feature,"feature");
  state.messages=params.messages;
  state.gadgetUseContext=params.gadgetUseContext;
  state.maxOutputTokensOverride=params.maxOutputTokensOverride;
  if(services.setup.sync.feature("TOKEN_BUDGET"))
    budgetTracker=createBudgetTracker(services.completion.budgetDependencies);
  config=buildQueryConfig(services.config);
  need(services.startRelevantMemoryPrefetch,"startRelevantMemoryPrefetch");
  memoryPrefetch=services.startRelevantMemoryPrefetch(state.messages,state.gadgetUseContext);
  startSetup();
}
void Run::startSetup(){
  setup.emplace(makeAsyncSetupIterator(loop,params,state,config,services.setup,remaining));
  stage=Stage::SetupStart;
}
void Run::pullSetup(){
  auto pending=setup->next();
  if(!loop.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Async setup belongs to another event loop");
  stage=Stage::SetupAwait;
  auto self=shared_from_this();
  pending.then([self](const AsyncSetupStep* value,const std::exception_ptr& error){
    if(self->stage==Stage::Done)return;
    if(error){self->fail(error);return;}
    try{
      if(value->done){
        if(!value->value)throw std::logic_error("Async setup lost prepared iteration");
        self->remaining=value->value->missionBudgetRemaining;
        self->prepared=std::make_shared<PreparedIteration>(
          std::move(value->value->iteration));
        self->setup.reset();
        if(self->prepared->terminal){self->finish(*self->prepared->terminal);return;}
        if(self->prepared->compacted&&self->services.refreshUserContextAfterCompact){
          self->refreshUserContext(false);return;
        }
        self->stage=Stage::ModelStart;
      }else{
        self->emit(std::make_shared<Json>(value->event),Stage::SetupStart);
        return;
      }
      self->pump();
    }catch(...){self->fail(std::current_exception());}
  });
}
void Run::refreshUserContext(bool restartSetup){
  const auto& context=restartSetup?state.gadgetUseContext:prepared->gadgetUseContext;
  auto pending=services.refreshUserContextAfterCompact(params.userContext,context);
  if(!loop.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Compact context refresh belongs to another event loop");
  stage=Stage::ContextAwait;
  auto self=shared_from_this();
  pending.then([self,restartSetup](const Json* value,const std::exception_ptr& error){
    if(self->stage==Stage::Done)return;
    if(error){self->fail(error);return;}
    try{
      if(!value||!value->is_object())
        throw std::invalid_argument("Compact context refresh requires an object");
      self->params.userContext=*value;
      if(restartSetup)self->startSetup();
      else self->stage=Stage::ModelStart;
      self->pump();
    }catch(...){self->fail(std::current_exception());}
  });
}
} // namespace batbox::query::async_engine
