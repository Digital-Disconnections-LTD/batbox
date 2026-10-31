#include "instructions/hooks/runtime/GatedGadgetWrappers.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
bool present(const GatedGadgetServices& services,
  const HookBatchContext& context,const char* event){
  if(!context.getAppState)
    throw std::invalid_argument("Hook app-state getter required");
  const Json appState=context.getAppState();
  std::string flightId;
  if(context.batId)flightId=*context.batId;
  else{
    if(!services.getFlightId)
      throw std::invalid_argument("Hook session ID import required");
    flightId=services.getFlightId();
  }
  if(!services.hasHookForEvent)
    throw std::invalid_argument("Hook presence import required");
  return services.hasHookForEvent(event,appState,flightId);
}
Json base(const GatedGadgetServices& services,
  const std::optional<std::string>& mode,
  const HookBatchContext& context){
  if(!services.wrapper.createBaseHookInput)
    throw std::invalid_argument("Base hook input import required");
  return services.wrapper.createBaseHookInput(mode,{},
    HookBatInfo{context.batId,context.batType});
}
ExecuteHooksInput invocation(Json hookInput,const std::string& id,
  const std::string& gadgetName,const HookBatchContext& context,
  std::optional<std::stop_token> signal,double timeoutMs){
  ExecuteHooksInput call;
  call.batch.hookInput=std::move(hookInput);
  call.batch.gadgetUseID=id;
  call.batch.matchQuery=gadgetName;
  call.batch.signal=signal;
  call.batch.gadgetUseContext=context;
  call.timeoutMs=timeoutMs;
  return call;
}
}
HookEventGenerator executePostGadgetUseFailureHooks(event_loop::Loop& loop,
  PostGadgetFailureCall call,GatedGadgetServices services){
  return HookEventGenerator(loop,[call=std::move(call),services]()mutable
    ->std::optional<ExecuteHooksInput>{
    if(!present(services,call.gadgetUseContext,"PostToolUseFailure"))return {};
    Json hookInput=base(services,call.permissionMode,call.gadgetUseContext);
    hookInput["hook_event_name"]="PostToolUseFailure";
    hookInput["tool_name"]=call.gadgetName;
    hookInput["tool_input"]=call.gadgetInput;
    hookInput["tool_use_id"]=call.gadgetUseID;
    hookInput["error"]=call.error;
    if(call.isInterrupt)hookInput["is_interrupt"]=*call.isInterrupt;
    return invocation(std::move(hookInput),call.gadgetUseID,call.gadgetName,
      call.gadgetUseContext,call.signal,call.timeoutMs);
  },services.wrapper.executeHooks);
}
HookEventGenerator executePermissionDeniedHooks(event_loop::Loop& loop,
  PermissionDeniedCall call,GatedGadgetServices services){
  return HookEventGenerator(loop,[call=std::move(call),services]()mutable
    ->std::optional<ExecuteHooksInput>{
    if(!present(services,call.gadgetUseContext,"PermissionDenied"))return {};
    Json hookInput=base(services,call.permissionMode,call.gadgetUseContext);
    hookInput["hook_event_name"]="PermissionDenied";
    hookInput["tool_name"]=call.gadgetName;
    hookInput["tool_input"]=call.gadgetInput;
    hookInput["tool_use_id"]=call.gadgetUseID;
    hookInput["reason"]=call.reason;
    return invocation(std::move(hookInput),call.gadgetUseID,call.gadgetName,
      call.gadgetUseContext,call.signal,call.timeoutMs);
  },services.wrapper.executeHooks);
}
} // namespace batbox::instructions::hooks::runtime
