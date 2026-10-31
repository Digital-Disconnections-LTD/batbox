#include "instructions/hooks/runtime/PoetryGadgetWrappers.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
std::string flight(const PoetryGadgetServices& services,
  const HookBatchContext& context){
  if(context.batId)return *context.batId;
  if(!services.getFlightId)
    throw std::invalid_argument("Hook session ID import required");
  return services.getFlightId();
}
Json base(const PoetryGadgetServices& services,
  const std::optional<std::string>& mode,
  const HookBatchContext& context){
  if(!services.wrapper.createBaseHookInput)
    throw std::invalid_argument("Base hook input import required");
  return services.wrapper.createBaseHookInput(mode,{},
    HookBatInfo{context.batId,context.batType});
}
ExecuteHooksInput input(Json hookInput,const std::string& id,
  const std::string& matchQuery,const HookBatchContext& context,
  std::optional<std::stop_token> signal,double timeoutMs,
  std::optional<Json> summary){
  ExecuteHooksInput call;
  call.batch.hookInput=std::move(hookInput);
  call.batch.gadgetUseID=id;
  call.batch.matchQuery=matchQuery;
  call.batch.signal=signal;
  call.batch.gadgetUseContext=context;
  call.batch.gadgetInputSummary=std::move(summary);
  call.timeoutMs=timeoutMs;
  return call;
}
std::function<HookYieldCursor(ExecuteHooksInput)> dispatcher(
  PoetryGadgetServices services,RequestPoetryFactory poetry){
  return [services=std::move(services),poetry=std::move(poetry)](
    ExecuteHooksInput call){
    if(!services.executeWithPoetry)
      throw std::invalid_argument("Prompt hook executor import required");
    return services.executeWithPoetry(std::move(call),poetry);
  };
}
}
HookEventGenerator executePreGadgetHooks(event_loop::Loop& loop,
  PreGadgetHookCall call,PoetryGadgetServices services){
  auto execute=dispatcher(services,call.requestPoetry);
  return HookEventGenerator(loop,[call=std::move(call),services]()mutable
    ->std::optional<ExecuteHooksInput>{
    if(!call.gadgetUseContext.getAppState)
      throw std::invalid_argument("Hook app-state getter required");
    const Json appState=call.gadgetUseContext.getAppState();
    const std::string flightId=flight(services,call.gadgetUseContext);
    if(!services.hasHookForEvent)
      throw std::invalid_argument("Hook presence import required");
    if(!services.hasHookForEvent("PreToolUse",appState,flightId))return {};
    if(!services.logForDebugging)
      throw std::invalid_argument("Hook local debug import required");
    services.logForDebugging("executePreToolHooks called for tool: "+
      call.gadgetName,Json{{"level","verbose"}});
    Json hookInput=base(services,call.permissionMode,call.gadgetUseContext);
    hookInput["hook_event_name"]="PreToolUse";
    hookInput["tool_name"]=call.gadgetName;
    hookInput["tool_input"]=call.gadgetInput;
    hookInput["tool_use_id"]=call.gadgetUseID;
    return input(std::move(hookInput),call.gadgetUseID,call.gadgetName,
      call.gadgetUseContext,call.signal,call.timeoutMs,call.gadgetInputSummary);
  },std::move(execute));
}
HookEventGenerator executePermissionRequestHooks(event_loop::Loop& loop,
  PermissionRequestHookCall call,PoetryGadgetServices services){
  auto execute=dispatcher(services,call.requestPoetry);
  return HookEventGenerator(loop,[call=std::move(call),services]()mutable
    ->std::optional<ExecuteHooksInput>{
    if(!services.logForDebugging)
      throw std::invalid_argument("Hook local debug import required");
    services.logForDebugging("executePermissionRequestHooks called for tool: "+
      call.gadgetName,{});
    Json hookInput=base(services,call.permissionMode,call.gadgetUseContext);
    hookInput["hook_event_name"]="PermissionRequest";
    hookInput["tool_name"]=call.gadgetName;
    hookInput["tool_input"]=call.gadgetInput;
    if(call.permissionSuggestions)
      hookInput["permission_suggestions"]=*call.permissionSuggestions;
    return input(std::move(hookInput),call.gadgetUseID,call.gadgetName,
      call.gadgetUseContext,call.signal,call.timeoutMs,call.gadgetInputSummary);
  },std::move(execute));
}
} // namespace batbox::instructions::hooks::runtime
