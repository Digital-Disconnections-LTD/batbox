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
  stage=Stage::Budget;
}
void Run::checkBudget(){
  need(services.sync.feature,"feature");
  if(services.sync.feature("TOKEN_BUDGET")){
    if(!budgetTracker)
      throw std::invalid_argument("Unbound async completion import: budgetTracker");
    need(services.sync.getCurrentTurnTokenBudget,"getCurrentTurnTokenBudget");
    need(services.sync.getTurnOutputTokens,"getTurnOutputTokens");
    const Json& bat=field(prepared->gadgetUseContext.data,"agentId");
    const auto batId=bat.is_string()?
      std::optional<std::string>(bat.get<std::string>()):std::nullopt;
    const Json decision=checkTokenBudget(*budgetTracker,batId,
      services.sync.getCurrentTurnTokenBudget(),services.sync.getTurnOutputTokens(),
      services.sync.budgetDependencies);
    if(field(decision,"action")=="continue"){
      need(services.sync.incrementBudgetContinuationCount,
        "incrementBudgetContinuationCount");
      services.sync.incrementBudgetContinuationCount();
      need(services.logForDebugging,"logForDebugging");
      need(services.localeNumber,"localeNumber");
      const auto number=[&](const char* key){
        const Json& value=field(decision,key);
        if(!value.is_number())throw std::invalid_argument(
          std::string("Token budget decision missing ")+key);
        return value.get<double>();
      };
      services.logForDebugging("Token budget continuation #"+
        value::numberToString(number("continuationCount"))+": "+
        value::numberToString(number("pct"))+"% ("+
        services.localeNumber(number("turnTokens"))+" / "+
        services.localeNumber(number("budget"))+")");
      need(services.sync.createUserMessage,"createUserMessage");
      const Json nudge=services.sync.createUserMessage(Json{{"content",
        field(decision,"nudgeMessage")},{"isMeta",true}});
      State next=nextState();
      Json history=appendMessageArrays(prepared->messagesForQuery,
        modelResult->assistantMessages);
      Json nudgeItems=Json::array();nudgeItems.push_back(nudge);
      next.messages=appendMessageArrays(history,nudgeItems);
      next.maxOutputTokensRecoveryCount=0;
      next.hasAttemptedReactiveCompact=false;
      next.maxOutputTokensOverride.reset();
      next.transition=Json{{"reason","token_budget_continuation"}};
      finish(CompletionDecision{next,{}});return;
    }
    const Json& completionEvent=field(decision,"completionEvent");
    if(value::truthy(field(completionEvent,"diminishingReturns"))){
      need(services.logForDebugging,"logForDebugging");
      const Json& pct=field(completionEvent,"pct");
      services.logForDebugging("Token budget early stop: diminishing returns at "+
        (pct.is_number()?value::numberToString(pct.get<double>()):
          std::string("undefined"))+"%");
    }
  }
  finish(CompletionDecision{{},Json{{"reason","completed"}}});
}
} // namespace batbox::query::async_completion
