#include "query/AsyncCompletionInternal.hpp"

namespace batbox::query::async_completion {
void Run::startStopHooks(){
  const Json& last=lastMessage();
  if(value::truthy(field(last,"isApiErrorMessage"))){
    need(services.sync.executeStopFailureHooks,"executeStopFailureHooks");
    services.sync.executeStopFailureHooks(last,prepared->gadgetUseContext);
    finish(CompletionDecision{{},Json{{"reason","completed"}}});return;
  }
  if(!stopCursor){
    need(services.handleStopHooks,"handleStopHooks");
    stopCursor=services.handleStopHooks(prepared->messagesForQuery,
      modelResult->assistantMessages,params,prepared->gadgetUseContext,
      state.stopHookActive);
    need(stopCursor->next,"stopHook cursor.next");
  }
  pullStopHooks();
}
void Run::pullStopHooks(){
  auto pending=stopCursor->next();
  if(!loop.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Async stop hooks belong to another event loop");
  stage=Stage::StopAwait;
  auto self=shared_from_this();
  pending.then([self](const AsyncStopStep* value,const std::exception_ptr& error){
    if(self->stage==Stage::Done)return;
    if(error){self->fail(error);return;}
    try{
      if(!value->done){self->emit(value->event,Stage::StopStart);return;}
      self->stopResult=value->result;
      self->stopCursor.reset();self->stage=Stage::AfterStop;self->pump();
    }catch(...){self->fail(std::current_exception());}
  });
}
void Run::afterStopHooks(){
  if(value::truthy(field(stopResult,"preventContinuation"))){
    finish(CompletionDecision{{},Json{{"reason","stop_hook_prevented"}}});return;
  }
  const Json& blocking=field(stopResult,"blockingErrors");
  if(blocking.is_array()&&!blocking.empty()){
    State next=nextState();
    Json firstJoined=appendMessageArrays(
      prepared->messagesForQuery,modelResult->assistantMessages);
    Json allJoined=appendMessageArrays(firstJoined,blocking);
    next.messages=std::move(allJoined);
    next.maxOutputTokensRecoveryCount=0;
    next.maxOutputTokensOverride.reset();
    next.stopHookActive=true;
    next.transition=Json{{"reason","stop_hook_blocking"}};
    finish(CompletionDecision{next,{}});return;
  }
  finish(CompletionDecision{{},Json{{"reason","completed"}}});
}
} // namespace batbox::query::async_completion
