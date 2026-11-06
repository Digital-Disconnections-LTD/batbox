#include "query/Completion.hpp"
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
  CompletionServices& services,const Yield& yield){
  need(yield,"yield");
  const Json& last=modelResult.assistantMessages.empty()?Json():modelResult.assistantMessages.back();
  if(isWithheldMaxOutputTokens(last)){
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
  return CompletionDecision{{},Json{{"reason","completed"}}};
}
} // namespace batbox::query
