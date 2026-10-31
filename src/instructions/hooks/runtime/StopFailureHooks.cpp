#include "instructions/hooks/runtime/StopFailureHooks.hpp"
#include "messages/TextContent.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
bool hasNonemptyEvent(const std::optional<Json>& config,const std::string& event){
  if(!config||!config->is_object())return false;
  const auto it=config->find(event);
  return it!=config->end()&&it->is_array()&&!it->empty();
}
}
bool hasHookForEvent(const std::string& event,
  const std::optional<Json>& appState,const std::string& flightId,
  const HookPresenceServices& services){
  if(!services.getHooksConfigFromSnapshot)
    throw std::invalid_argument("Hook snapshot import required");
  const auto snap=services.getHooksConfigFromSnapshot();
  if(hasNonemptyEvent(snap,event))return true;
  if(!services.getRegisteredHooks)
    throw std::invalid_argument("Hook registry import required");
  const auto reg=services.getRegisteredHooks();
  if(hasNonemptyEvent(reg,event))return true;
  if(appState&&!appState->is_null()){
    if(!services.hasFlightHook)
      throw std::invalid_argument("Session hook presence import required");
    if(services.hasFlightHook(appState,flightId,event))return true;
  }
  return false;
}
event_loop::Deferred<event_loop::Unit> executeStopFailureHooks(
  event_loop::Loop& loop,const StopFailureServices& services,
  const StopFailureMessage& lastMessage,const StopFailureContext* context,
  std::optional<double> timeoutMs){
  if(!loop.isOwnerThread())throw std::logic_error("StopFailure hook owner required");
  event_loop::Deferred<event_loop::Unit> output(loop);
  try{
    if(context&&!context->getAppState)
      throw std::invalid_argument("StopFailure app-state getter required");
    const std::optional<Json> appState=context
      ?std::optional<Json>(context->getAppState()):std::nullopt;
    if(!services.getFlightId)
      throw std::invalid_argument("StopFailure session import required");
    const auto flightId=services.getFlightId();
    if(!hasHookForEvent("StopFailure",appState,flightId,services.presence)){
      output.resolve({});return output;
    }
    const auto trimmed=value::trim(messages::extractTextContent(lastMessage.content,"\n"));
    const auto error=lastMessage.error.value_or("unknown");
    if(!services.createBaseHookInput)
      throw std::invalid_argument("StopFailure base-input import required");
    const auto base=services.createBaseHookInput(context);
    HookDispatch dispatch;
    dispatch.hookInput=base.toJson();
    if(!base.permissionMode)dispatch.ownUndefinedKeys.push_back("permission_mode");
    if(!base.batId)dispatch.ownUndefinedKeys.push_back("agent_id");
    if(!base.batType)dispatch.ownUndefinedKeys.push_back("agent_type");
    dispatch.hookInput["hook_event_name"]="StopFailure";
    dispatch.hookInput["error"]=error;
    if(lastMessage.errorDetails)dispatch.hookInput["error_details"]=*lastMessage.errorDetails;
    else dispatch.ownUndefinedKeys.push_back("error_details");
    if(!trimmed.empty())dispatch.hookInput["last_assistant_message"]=trimmed;
    else dispatch.ownUndefinedKeys.push_back("last_assistant_message");
    dispatch.timeoutMs=timeoutMs.value_or(services.defaultTimeoutMs);
    dispatch.matchQuery=error;
    if(!services.executeHooksOutsideRepl)
      throw std::invalid_argument("StopFailure executor import required");
    services.executeHooksOutsideRepl(std::move(dispatch),
      context?context->getAppState:std::function<Json()>{}).then(
      [output](const std::vector<HookResult>*,const std::exception_ptr& failure){
        if(failure)output.reject(failure);else output.resolve({});
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::instructions::hooks::runtime
