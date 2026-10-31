#include "query/AsyncCompletionInternal.hpp"

namespace batbox::query {
AsyncDecisionCursor makeAsyncCompletionCursor(event_loop::Loop& loop,
  const Params& params,const State& state,
  std::shared_ptr<PreparedIteration> prepared,
  std::shared_ptr<ModelPassResult> modelResult,
  std::optional<double>& missionBudgetRemaining,BudgetTracker* budgetTracker,
  AsyncCompletionServices services){
  if(!loop.isOwnerThread())throw std::logic_error("Async completion requires loop owner");
  if(!prepared||!modelResult)throw std::invalid_argument("Async completion requires model pass");
  auto run=std::make_shared<async_completion::Run>(loop,params,state,
    std::move(prepared),std::move(modelResult),missionBudgetRemaining,budgetTracker,
    std::move(services));
  return AsyncDecisionCursor{[run]{return run->next();},
    [run]{return run->close();}};
}
void bindAsyncCompletion(event_loop::Loop& loop,AsyncEngineServices& engine,
  AsyncCompletionServices services){
  engine.completeWithoutGadgets=[&loop,services=std::move(services)](
      const Params& params,const State& state,
      std::shared_ptr<PreparedIteration> prepared,
      std::shared_ptr<ModelPassResult> modelResult,
      std::optional<double>& remaining,BudgetTracker* budgetTracker){
    return makeAsyncCompletionCursor(loop,params,state,std::move(prepared),
      std::move(modelResult),remaining,budgetTracker,services);
  };
}
} // namespace batbox::query

namespace batbox::query::async_completion {
event_loop::Deferred<AsyncDecisionStep> Run::next(){
  if(!loop.isOwnerThread())throw std::logic_error("Async completion next() requires loop owner");
  event_loop::Deferred<AsyncDecisionStep> request(loop);
  requests.push_back(request);pump();return request;
}
void Run::emit(const model::MessageRef& event,Stage after){
  stage=after;pausedAfterYield=true;
  auto request=requests.front();requests.pop_front();
  request.resolve(AsyncDecisionStep{false,event,{}});
  if(!requests.empty()){
    auto self=shared_from_this();loop.queueMicrotask([self]{self->pump();});
  }
}
void Run::finish(CompletionDecision decision){
  stage=Stage::Done;
  auto request=requests.front();requests.pop_front();
  request.resolve(AsyncDecisionStep{true,{},
    std::make_shared<CompletionDecision>(std::move(decision))});
  while(!requests.empty()){
    auto queued=requests.front();requests.pop_front();
    queued.resolve(AsyncDecisionStep{true,{},{}});
  }
}
void Run::fail(std::exception_ptr error){
  failure=std::move(error);stage=Stage::Done;
  if(!requests.empty()){
    auto request=requests.front();requests.pop_front();request.reject(failure);
  }
  while(!requests.empty()){
    auto queued=requests.front();requests.pop_front();
    queued.resolve(AsyncDecisionStep{true,{},{}});
  }
}
void Run::pump(){
  if(requests.empty())return;
  try{
    while(!requests.empty()){
      switch(stage){
        case Stage::Start:start();break;
        case Stage::AwaitReactive:case Stage::StopAwait:return;
        case Stage::PostCompact:emitPostCompact();break;
        case Stage::AfterPostCompact:finishReactive();break;
        case Stage::AfterWithheldError:afterWithheldError();break;
        case Stage::AfterExhaustedMax:stage=Stage::StopStart;break;
        case Stage::StopStart:startStopHooks();break;
        case Stage::AfterStop:afterStopHooks();break;
        case Stage::Budget:checkBudget();break;
        case Stage::Done:{
          auto request=requests.front();requests.pop_front();
          request.resolve(AsyncDecisionStep{true,{},{}});break;
        }
      }
      if(pausedAfterYield){pausedAfterYield=false;return;}
    }
  }catch(...){fail(std::current_exception());}
}
const Json& Run::lastMessage()const{
  static const Json empty;
  return modelResult->assistantMessages.empty()?empty:modelResult->assistantMessages.back();
}
model::MessageRef Run::lastMessageRef()const{
  if(!modelResult->assistantRefs.empty())return modelResult->assistantRefs.back();
  return std::make_shared<Json>(lastMessage());
}
State Run::nextState()const{
  State next=state;
  next.gadgetUseContext=prepared->gadgetUseContext;
  next.autoCompactTracking=prepared->tracking;
  next.pendingGadgetUseSummary.reset();
  next.pendingGadgetUseSummaryAsync.reset();
  next.stopHookActive.reset();
  return next;
}
void Run::start(){
  auto& sync=services.sync;
  need(sync.feature,"feature");
  const Json& last=lastMessage();
  withheld413=false;
  if(field(last,"type")=="assistant"&&value::truthy(field(last,"isApiErrorMessage"))){
    need(sync.isPoetryTooLongMessage,"isPromptTooLongMessage");
    withheld413=sync.isPoetryTooLongMessage(last);
  }
  need(services.reactiveCompactAvailable,"reactiveCompactAvailable");
  const bool reactive=services.reactiveCompactAvailable();
  withheldMedia=false;
  if(prepared->mediaRecoveryEnabled&&reactive){
    need(sync.isWithheldMediaSizeError,"isWithheldMediaSizeError");
    withheldMedia=sync.isWithheldMediaSizeError(last);
  }
  if(withheld413&&sync.feature("CONTEXT_COLLAPSE")&&
     field(state.transition,"reason")!="collapse_drain_retry"){
    need(services.contextCollapseAvailable,"contextCollapseAvailable");
    if(services.contextCollapseAvailable()){
      need(sync.recoverFromOverflow,"recoverFromOverflow");
      const Json drained=sync.recoverFromOverflow(prepared->messagesForQuery,
        params.querySource);
      const Json& committed=field(drained,"committed");
      if(committed.is_number()&&committed.get<double>()>0){
        State next=nextState();next.messages=field(drained,"messages");
        next.maxOutputTokensOverride.reset();
        next.transition=Json{{"reason","collapse_drain_retry"},
          {"committed",committed}};
        finish(CompletionDecision{next,{}});return;
      }
    }
  }
  if((withheld413||withheldMedia)&&reactive){startReactive();return;}
  if(withheld413&&sync.feature("CONTEXT_COLLAPSE")){
    emit(lastMessageRef(),Stage::AfterWithheldError);return;
  }
  stage=Stage::AfterExhaustedMax;
  afterExhaustedMax();
}
} // namespace batbox::query::async_completion
