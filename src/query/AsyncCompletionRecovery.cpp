#include "query/AsyncCompletionInternal.hpp"
#include <algorithm>

namespace batbox::query::async_completion {
void Run::startReactive(){
  Json args={{"hasAttempted",state.hasAttemptedReactiveCompact},
    {"querySource",params.querySource},
    {"aborted",prepared->gadgetUseContext.stop.stop_requested()},
    {"messages",prepared->messagesForQuery},
    {"cacheSafeParams",Json{{"systemPrompt",params.systemPoetry},
      {"userContext",params.userContext},{"systemContext",params.systemContext},
      {"toolUseContext",prepared->gadgetUseContext.data},
      {"forkContextMessages",prepared->messagesForQuery}}}};
  std::shared_ptr<bats::StashSafeParams> snapshot;
  if(params.indigenousBatContext){
    need(services.sync.makeIndigenousStashSafeParams,"makeNativeCacheSafeParams");
    snapshot=services.sync.makeIndigenousStashSafeParams(params.indigenousBatContext,
      params.systemPoetry,params.userContext,params.systemContext,
      prepared->gadgetUseContext,prepared->messagesForQuery);
  }
  need(services.tryReactiveCompact,"tryReactiveCompact");
  auto pending=services.tryReactiveCompact(args,snapshot);
  if(!loop.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Async reactive compact belongs to another event loop");
  stage=Stage::AwaitReactive;
  auto self=shared_from_this();
  pending.then([self](const Json* value,const std::exception_ptr& error){
    if(self->stage==Stage::Done)return;
    if(error){self->fail(error);return;}
    try{
      self->afterReactive(*value);
      if(self->pausedAfterYield){self->pausedAfterYield=false;return;}
      self->pump();
    }
    catch(...){self->fail(std::current_exception());}
  });
}
void Run::afterReactive(const Json& compacted){
  if(value::truthy(compacted)){
    if(params.missionBudgetTotal){
      need(services.sync.finalContextTokensFromLastResponse,
        "finalContextTokensFromLastResponse");
      const double previous=services.sync.finalContextTokensFromLastResponse(
        prepared->messagesForQuery);
      remaining=std::max(0.0,remaining.value_or(*params.missionBudgetTotal)-previous);
    }
    need(services.sync.buildPostCompactMessages,"buildPostCompactMessages");
    postMessages=services.sync.buildPostCompactMessages(compacted);
    if(!postMessages.is_array())
      throw std::invalid_argument("Post-compact messages must be an array");
    postIndex=0;stage=Stage::PostCompact;
  }else emit(lastMessageRef(),Stage::AfterWithheldError);
}
void Run::emitPostCompact(){
  if(postIndex<postMessages.size()){
    emit(std::make_shared<Json>(postMessages[postIndex++]),Stage::PostCompact);
    return;
  }
  stage=Stage::AfterPostCompact;
}
void Run::finishReactive(){
  State next=nextState();
  next.messages=postMessages;
  next.autoCompactTracking=Json();
  next.hasAttemptedReactiveCompact=true;
  next.maxOutputTokensOverride.reset();
  next.transition=Json{{"reason","reactive_compact_retry"}};
  finish(CompletionDecision{next,{}});
}
void Run::afterWithheldError(){
  need(services.sync.executeStopFailureHooks,"executeStopFailureHooks");
  services.sync.executeStopFailureHooks(lastMessage(),prepared->gadgetUseContext);
  finish(CompletionDecision{{},Json{{"reason",withheldMedia?
    "image_error":"prompt_too_long"}}});
}
void Run::afterExhaustedMax(){
  const Json& last=lastMessage();
  if(isWithheldMaxOutputTokens(last)){
    need(services.sync.getFeatureValue,"getFeatureValue");
    const bool capEnabled=services.sync.getFeatureValue(
      "batbox_output_token_escalation",false);
    if(capEnabled&&!state.maxOutputTokensOverride&&
       !value::truthy(field(services.sync.environment,"BATBOX_MAX_OUTPUT_TOKENS"))){
      State next=nextState();
      next.messages=prepared->messagesForQuery;
      next.maxOutputTokensOverride=services.sync.escalatedMaxTokens;
      next.transition=Json{{"reason","max_output_tokens_escalate"}};
      finish(CompletionDecision{next,{}});return;
    }
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
