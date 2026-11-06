#include "query/AsyncCompletionInternal.hpp"

namespace batbox::query {
AsyncDecisionCursor makeAsyncCompletionCursor(event_loop::Loop& loop,
  const Params& params,const State& state,
  std::shared_ptr<PreparedIteration> prepared,
  std::shared_ptr<ModelPassResult> modelResult,
  AsyncCompletionServices services){
  if(!loop.isOwnerThread())throw std::logic_error("Async completion requires loop owner");
  if(!prepared||!modelResult)throw std::invalid_argument("Async completion requires model pass");
  auto run=std::make_shared<async_completion::Run>(loop,params,state,
    std::move(prepared),std::move(modelResult),std::move(services));
  return AsyncDecisionCursor{[run]{return run->next();},
    [run]{return run->close();}};
}
void bindAsyncCompletion(event_loop::Loop& loop,AsyncEngineServices& engine,
  AsyncCompletionServices services){
  engine.completeWithoutGadgets=[&loop,services=std::move(services)](
      const Params& params,const State& state,
      std::shared_ptr<PreparedIteration> prepared,
      std::shared_ptr<ModelPassResult> modelResult){
    return makeAsyncCompletionCursor(loop,params,state,std::move(prepared),
      std::move(modelResult),services);
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
        case Stage::StopAwait:return;
        case Stage::StopStart:startStopHooks();break;
        case Stage::AfterStop:afterStopHooks();break;
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
  next.stopHookActive.reset();
  return next;
}
void Run::start(){
  const Json& last=lastMessage();
  if(isWithheldMaxOutputTokens(last)){
    if(state.maxOutputTokensRecoveryCount<3){
      need(services.sync.createUserMessage,"createUserMessage");
      const Json recovery=services.sync.createUserMessage(Json{{"content",
        "Output token limit hit. Resume directly — no apology, no recap of what you were doing. "
        "Pick up mid-thought if that is where the cut happened. Break remaining work into smaller pieces."},
        {"isMeta",true}});
      State next=nextState();
      Json history=appendMessageArrays(prepared->messagesForQuery,
        modelResult->assistantMessages);
      Json recoveryItems=Json::array();recoveryItems.push_back(recovery);
      next.messages=appendMessageArrays(history,recoveryItems);
      next.maxOutputTokensRecoveryCount=state.maxOutputTokensRecoveryCount+1;
      next.maxOutputTokensOverride.reset();
      next.transition=Json{{"reason","max_output_tokens_recovery"},
        {"attempt",next.maxOutputTokensRecoveryCount}};
      finish(CompletionDecision{next,{}});return;
    }
    emit(lastMessageRef(),Stage::StopStart);return;
  }
  stage=Stage::StopStart;
}
} // namespace batbox::query::async_completion
