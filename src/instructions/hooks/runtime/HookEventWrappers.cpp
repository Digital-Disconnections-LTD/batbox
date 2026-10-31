#include "instructions/hooks/runtime/HookEventWrappers.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
HookYield closedYield(std::optional<Json> value={}){
  return {true,value.value_or(Json(nullptr)),
    value?std::vector<std::string>{}:std::vector<std::string>{"value"}};
}
std::string uuid(const HookEventWrapperServices& services){
  if(!services.randomUUID)throw std::invalid_argument("Hook UUID import required");
  return services.randomUUID();
}
Json base(const HookEventWrapperServices& services,
  std::optional<std::string> mode={},std::optional<std::string> flight={}){
  if(!services.createBaseHookInput)
    throw std::invalid_argument("Base hook input import required");
  return services.createBaseHookInput(std::move(mode),std::move(flight),{});
}
ExecuteHooksInput input(Json hookInput,const std::string& id,
  std::optional<std::string> matchQuery,std::optional<std::stop_token> signal,
  double timeoutMs,bool forceSyncExecution){
  ExecuteHooksInput call;
  call.batch.hookInput=std::move(hookInput);
  call.batch.gadgetUseID=id;
  call.batch.matchQuery=std::move(matchQuery);
  call.batch.signal=signal;
  call.timeoutMs=timeoutMs;
  call.forceSyncExecution=forceSyncExecution;
  return call;
}
}
HookEventGenerator::HookEventGenerator(event_loop::Loop& loop,
  std::function<std::optional<ExecuteHooksInput>()> prepare,
  std::function<HookYieldCursor(ExecuteHooksInput)> executeHooks):
  loop_(loop),prepare_(std::move(prepare)),
  executeHooks_(std::move(executeHooks)){}
event_loop::Deferred<HookYield> HookEventGenerator::next(){
  if(!loop_.isOwnerThread())throw std::logic_error("Hook wrapper owner required");
  event_loop::Deferred<HookYield> output(loop_);
  if(closed_){output.resolve(closedYield());return output;}
  try{
    if(!cursor_){
      if(!prepare_||!executeHooks_)
        throw std::invalid_argument("Hook wrapper executor import required");
      auto invocation=prepare_();
      if(!invocation){closed_=true;output.resolve(closedYield());return output;}
      cursor_=executeHooks_(std::move(*invocation));
    }
    return cursor_->next();
  }catch(...){closed_=true;output.reject(std::current_exception());return output;}
}
event_loop::Deferred<HookYield> HookEventGenerator::returnValue(
  std::optional<Json> value){
  if(!loop_.isOwnerThread())throw std::logic_error("Hook wrapper owner required");
  if(cursor_)return cursor_->returnValue(std::move(value));
  closed_=true;
  event_loop::Deferred<HookYield> output(loop_);
  output.resolve(closedYield(std::move(value)));
  return output;
}
event_loop::Deferred<HookYield> HookEventGenerator::throwError(
  std::exception_ptr error){
  if(!error)throw std::invalid_argument("Hook generator throw error required");
  if(!loop_.isOwnerThread())throw std::logic_error("Hook wrapper owner required");
  if(cursor_)return cursor_->throwError(error);
  closed_=true;
  event_loop::Deferred<HookYield> output(loop_);
  output.reject(error);
  return output;
}
HookEventGenerator executeFlightStartHooks(event_loop::Loop& loop,
  FlightStartHookCall call,HookEventWrapperServices services){
  return HookEventGenerator(loop,[call=std::move(call),services]()mutable{
    Json hookInput=base(services,{},call.flightId);
    hookInput["hook_event_name"]="SessionStart";
    hookInput["source"]=call.source;
    if(call.batType)hookInput["agent_type"]=*call.batType;
    else hookInput.erase("agent_type");
    if(call.model)hookInput["model"]=*call.model;
    return input(std::move(hookInput),uuid(services),call.source,
      call.signal,call.timeoutMs,call.forceSyncExecution);
  },services.executeHooks);
}
HookEventGenerator executeSetupHooks(event_loop::Loop& loop,
  SetupHookCall call,HookEventWrapperServices services){
  return HookEventGenerator(loop,[call=std::move(call),services]()mutable{
    Json hookInput=base(services);
    hookInput["hook_event_name"]="Setup";
    hookInput["trigger"]=call.trigger;
    return input(std::move(hookInput),uuid(services),call.trigger,
      call.signal,call.timeoutMs,call.forceSyncExecution);
  },services.executeHooks);
}
HookEventGenerator executeSubagentStartHooks(event_loop::Loop& loop,
  SubagentStartHookCall call,HookEventWrapperServices services){
  return HookEventGenerator(loop,[call=std::move(call),services]()mutable{
    Json hookInput=base(services);
    hookInput["hook_event_name"]="SubagentStart";
    hookInput["agent_id"]=call.batId;
    hookInput["agent_type"]=call.batType;
    return input(std::move(hookInput),uuid(services),call.batType,
      call.signal,call.timeoutMs,false);
  },services.executeHooks);
}
} // namespace batbox::instructions::hooks::runtime
