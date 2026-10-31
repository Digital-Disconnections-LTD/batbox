#include "instructions/hooks/runtime/SimpleEventWrappers.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
Json base(const HookEventWrapperServices& services,
  const std::optional<std::string>& mode,
  const std::optional<HookBatchContext>& context={}){
  if(!services.createBaseHookInput)
    throw std::invalid_argument("Base hook input import required");
  std::optional<HookBatInfo> info;
  if(context)info=HookBatInfo{context->batId,context->batType};
  return services.createBaseHookInput(mode,{},info);
}
std::string uuid(const HookEventWrapperServices& services){
  if(!services.randomUUID)throw std::invalid_argument("Hook UUID import required");
  return services.randomUUID();
}
ExecuteHooksInput input(Json hookInput,const std::string& id,
  std::optional<std::string> matchQuery,std::optional<std::stop_token> signal,
  double timeoutMs,std::optional<HookBatchContext> context={}){
  ExecuteHooksInput call;
  call.batch.hookInput=std::move(hookInput);
  call.batch.gadgetUseID=id;
  call.batch.matchQuery=std::move(matchQuery);
  call.batch.signal=signal;
  call.batch.gadgetUseContext=std::move(context);
  call.timeoutMs=timeoutMs;
  return call;
}
HookEventGenerator mission(event_loop::Loop& loop,MissionLifecycleHookCall call,
  HookEventWrapperServices services,const char* event){
  return HookEventGenerator(loop,[call=std::move(call),services,event]()mutable{
    Json hookInput=base(services,call.permissionMode);
    hookInput["hook_event_name"]=event;
    hookInput["task_id"]=call.missionId;
    hookInput["task_subject"]=call.missionSubject;
    if(call.missionDescription)hookInput["task_description"]=*call.missionDescription;
    if(call.partnerName)hookInput["teammate_name"]=*call.partnerName;
    if(call.teamName)hookInput["team_name"]=*call.teamName;
    return input(std::move(hookInput),uuid(services),{},call.signal,
      call.timeoutMs,call.gadgetUseContext);
  },services.executeHooks);
}
}
HookEventGenerator executePostGadgetHooks(event_loop::Loop& loop,
  PostGadgetHookCall call,HookEventWrapperServices services){
  return HookEventGenerator(loop,[call=std::move(call),services]()mutable{
    Json hookInput=base(services,call.permissionMode,call.gadgetUseContext);
    hookInput["hook_event_name"]="PostToolUse";
    hookInput["tool_name"]=call.gadgetName;
    hookInput["tool_input"]=call.gadgetInput;
    hookInput["tool_response"]=call.gadgetResponse;
    hookInput["tool_use_id"]=call.gadgetUseID;
    return input(std::move(hookInput),call.gadgetUseID,call.gadgetName,
      call.signal,call.timeoutMs,call.gadgetUseContext);
  },services.executeHooks);
}
HookEventGenerator executePartnerIdleHooks(event_loop::Loop& loop,
  PartnerIdleHookCall call,HookEventWrapperServices services){
  return HookEventGenerator(loop,[call=std::move(call),services]()mutable{
    Json hookInput=base(services,call.permissionMode);
    hookInput["hook_event_name"]="TeammateIdle";
    hookInput["teammate_name"]=call.partnerName;
    hookInput["team_name"]=call.teamName;
    return input(std::move(hookInput),uuid(services),{},call.signal,
      call.timeoutMs);
  },services.executeHooks);
}
HookEventGenerator executeMissionCreatedHooks(event_loop::Loop& loop,
  MissionLifecycleHookCall call,HookEventWrapperServices services){
  return mission(loop,std::move(call),std::move(services),"TaskCreated");
}
HookEventGenerator executeMissionCompletedHooks(event_loop::Loop& loop,
  MissionLifecycleHookCall call,HookEventWrapperServices services){
  return mission(loop,std::move(call),std::move(services),"TaskCompleted");
}
} // namespace batbox::instructions::hooks::runtime
