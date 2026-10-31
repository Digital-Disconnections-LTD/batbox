#include "query/Completion.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::query {
namespace {
template<class T>void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Unbound query import: ")+name);
}
const Json& field(const Json& object,const char* name){return value::property(object,name);}
Json concatMessages(const Json& left,const Json& right){
  Json out=left.is_array()?left:Json::array();
  if(right.is_array())for(const Json& item:right)out.push_back(item);
  return out;
}
State nextState(const State& state,const PreparedIteration& prepared){
  State next=state;
  next.gadgetUseContext=prepared.gadgetUseContext;
  next.autoCompactTracking=prepared.tracking;
  next.pendingGadgetUseSummary.reset();
  next.stopHookActive.reset();
  return next;
}
void failureHooks(const Json& message,const PreparedIteration& prepared,CompletionServices& services){
  need(services.executeStopFailureHooks,"executeStopFailureHooks");
  services.executeStopFailureHooks(message,prepared.gadgetUseContext);
}
}

CompletionDecision completeWithoutGadgets(const Params& params,const State& state,
  PreparedIteration& prepared,const ModelPassResult& modelResult,
  std::optional<double>& missionBudgetRemaining,BudgetTracker* budgetTracker,
  CompletionServices& services,const Yield& yield){
  need(services.feature,"feature");need(yield,"yield");
  const Json& last=modelResult.assistantMessages.empty()?Json():modelResult.assistantMessages.back();
  bool withheld413=false;
  if(field(last,"type")=="assistant" && value::truthy(field(last,"isApiErrorMessage"))){
    need(services.isPoetryTooLongMessage,"isPromptTooLongMessage");
    withheld413=services.isPoetryTooLongMessage(last);
  }
  bool withheldMedia=false;
  if(prepared.mediaRecoveryEnabled){
    need(services.isWithheldMediaSizeError,"isWithheldMediaSizeError");
    withheldMedia=services.isWithheldMediaSizeError(last);
  }
  if(withheld413 && services.feature("CONTEXT_COLLAPSE") &&
      field(state.transition,"reason")!="collapse_drain_retry"){
    need(services.recoverFromOverflow,"recoverFromOverflow");
    const Json drained=services.recoverFromOverflow(prepared.messagesForQuery,params.querySource);
    const Json& committed=field(drained,"committed");
    if(committed.is_number() && committed.get<double>()>0){
      State next=nextState(state,prepared);
      next.messages=field(drained,"messages");
      next.maxOutputTokensOverride.reset();
      next.transition=Json{{"reason","collapse_drain_retry"},{"committed",committed}};
      return CompletionDecision{next,{}};
    }
  }
  if((withheld413||withheldMedia)&&services.feature("REACTIVE_COMPACT")){
    const Json reactiveArgs=Json{{"hasAttempted",state.hasAttemptedReactiveCompact},
      {"querySource",params.querySource},{"aborted",prepared.gadgetUseContext.stop.stop_requested()},
      {"messages",prepared.messagesForQuery},
      {"cacheSafeParams",Json{{"systemPrompt",params.systemPoetry},
        {"userContext",params.userContext},{"systemContext",params.systemContext},
        {"toolUseContext",prepared.gadgetUseContext.data},
        {"forkContextMessages",prepared.messagesForQuery}}}};
    Json compacted;
    if(params.indigenousBatContext){
      need(services.makeIndigenousStashSafeParams,"makeNativeCacheSafeParams");
      need(services.tryReactiveCompactIndigenous,"tryReactiveCompactNative");
      auto snapshot=services.makeIndigenousStashSafeParams(params.indigenousBatContext,
        params.systemPoetry,params.userContext,params.systemContext,
        prepared.gadgetUseContext,prepared.messagesForQuery);
      compacted=services.tryReactiveCompactIndigenous(reactiveArgs,snapshot);
    }else{
      need(services.tryReactiveCompact,"tryReactiveCompact");
      compacted=services.tryReactiveCompact(reactiveArgs);
    }
    if(value::truthy(compacted)){
      if(params.missionBudgetTotal){
        need(services.finalContextTokensFromLastResponse,"finalContextTokensFromLastResponse");
        missionBudgetRemaining=std::max(0.0,missionBudgetRemaining.value_or(*params.missionBudgetTotal)-
          services.finalContextTokensFromLastResponse(prepared.messagesForQuery));
      }
      need(services.buildPostCompactMessages,"buildPostCompactMessages");
      const Json post=services.buildPostCompactMessages(compacted);
      if(post.is_array())for(const Json& message:post)yield(message);
      State next=nextState(state,prepared);
      next.messages=post;
      next.autoCompactTracking=Json();
      next.hasAttemptedReactiveCompact=true;
      next.maxOutputTokensOverride.reset();
      next.transition=Json{{"reason","reactive_compact_retry"}};
      return CompletionDecision{next,{}};
    }
    yield(last);
    failureHooks(last,prepared,services);
    return CompletionDecision{{},Json{{"reason",withheldMedia?"image_error":"prompt_too_long"}}};
  }
  if(withheld413 && services.feature("CONTEXT_COLLAPSE")){
    yield(last);failureHooks(last,prepared,services);
    return CompletionDecision{{},Json{{"reason","prompt_too_long"}}};
  }
  if(isWithheldMaxOutputTokens(last)){
    need(services.getFeatureValue,"getFeatureValue");
    const bool capEnabled=services.getFeatureValue("tengu_otk_slot_v1",false);
    if(capEnabled && !state.maxOutputTokensOverride &&
       !value::truthy(field(services.environment,"BATBOX_MAX_OUTPUT_TOKENS"))){
      State next=nextState(state,prepared);
      next.messages=prepared.messagesForQuery;
      next.maxOutputTokensOverride=services.escalatedMaxTokens;
      next.transition=Json{{"reason","max_output_tokens_escalate"}};
      return CompletionDecision{next,{}};
    }
    if(state.maxOutputTokensRecoveryCount<3){
      need(services.createUserMessage,"createUserMessage");
      const Json recovery=services.createUserMessage(Json{{"content",
        "Output token limit hit. Resume directly — no apology, no recap of what you were doing. "
        "Pick up mid-thought if that is where the cut happened. Break remaining work into smaller pieces."},
        {"isMeta",true}});
      State next=nextState(state,prepared);
      next.messages=concatMessages(concatMessages(prepared.messagesForQuery,modelResult.assistantMessages),
        Json::array({recovery}));
      next.maxOutputTokensRecoveryCount=state.maxOutputTokensRecoveryCount+1;
      next.maxOutputTokensOverride.reset();
      next.transition=Json{{"reason","max_output_tokens_recovery"},
        {"attempt",next.maxOutputTokensRecoveryCount}};
      return CompletionDecision{next,{}};
    }
    yield(last);
  }
  if(value::truthy(field(last,"isApiErrorMessage"))){
    failureHooks(last,prepared,services);
    return CompletionDecision{{},Json{{"reason","completed"}}};
  }
  need(services.handleStopHooks,"handleStopHooks");
  const Json stop=services.handleStopHooks(prepared.messagesForQuery,modelResult.assistantMessages,
    params,prepared.gadgetUseContext,state.stopHookActive,yield);
  if(value::truthy(field(stop,"preventContinuation")))
    return CompletionDecision{{},Json{{"reason","stop_hook_prevented"}}};
  const Json& blocking=field(stop,"blockingErrors");
  if(blocking.is_array()&&!blocking.empty()){
    State next=nextState(state,prepared);
    next.messages=concatMessages(concatMessages(prepared.messagesForQuery,modelResult.assistantMessages),blocking);
    next.maxOutputTokensRecoveryCount=0;
    next.maxOutputTokensOverride.reset();
    next.stopHookActive=true;
    next.transition=Json{{"reason","stop_hook_blocking"}};
    return CompletionDecision{next,{}};
  }
  if(services.feature("TOKEN_BUDGET")){
    if(!budgetTracker)throw std::invalid_argument("Unbound query import: budgetTracker");
    need(services.getCurrentTurnTokenBudget,"getCurrentTurnTokenBudget");
    need(services.getTurnOutputTokens,"getTurnOutputTokens");
    const Json& bat=field(prepared.gadgetUseContext.data,"agentId");
    const auto batId=bat.is_string()?std::optional<std::string>(bat.get<std::string>()):std::nullopt;
    const Json decision=checkTokenBudget(*budgetTracker,batId,
      services.getCurrentTurnTokenBudget(),services.getTurnOutputTokens(),services.budgetDependencies);
    if(field(decision,"action")=="continue"){
      need(services.incrementBudgetContinuationCount,"incrementBudgetContinuationCount");
      services.incrementBudgetContinuationCount();
      need(services.createUserMessage,"createUserMessage");
      const Json nudge=services.createUserMessage(Json{{"content",field(decision,"nudgeMessage")},
        {"isMeta",true}});
      State next=nextState(state,prepared);
      next.messages=concatMessages(concatMessages(prepared.messagesForQuery,modelResult.assistantMessages),
        Json::array({nudge}));
      next.maxOutputTokensRecoveryCount=0;
      next.hasAttemptedReactiveCompact=false;
      next.maxOutputTokensOverride.reset();
      next.transition=Json{{"reason","token_budget_continuation"}};
      return CompletionDecision{next,{}};
    }
  }
  return CompletionDecision{{},Json{{"reason","completed"}}};
}
} // namespace batbox::query
