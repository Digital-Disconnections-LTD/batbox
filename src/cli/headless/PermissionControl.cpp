#include "cli/headless/PermissionControl.hpp"
#include <stdexcept>

namespace batbox::cli::headless {
namespace {
Json errorResponse(const std::string& requestId,const std::string& error){
  return Json{{"type","control_response"},{"response",Json{
    {"subtype","error"},{"request_id",requestId},{"error",error}}}};
}
Json successResponse(const std::string& requestId,const Json& mode){
  return Json{{"type","control_response"},{"response",Json{
    {"subtype","success"},{"request_id",requestId},
    {"response",Json{{"mode",mode}}}}}};
}
}
Json handleSetPermissionMode(const Json& request,const std::string& requestId,
    const Json& gadgetPermissionContext,const PermissionControlServices& services){
  if(!services.isBypassPermissionsModeDisabled||
     !services.transcriptClassifierEnabled||!services.isAutoModeGateEnabled||
     !services.getAutoModeUnavailableReason||
     !services.getAutoModeUnavailableNotification||
     !services.transitionPermissionMode||!services.enqueue)
    throw std::invalid_argument("Permission control imports required");
  const auto& mode=value::property(request,"mode");
  if(mode=="bypassPermissions"){
    if(services.isBypassPermissionsModeDisabled()){
      services.enqueue(errorResponse(requestId,
        "Cannot set permission mode to bypassPermissions because it is disabled by settings or configuration"));
      return gadgetPermissionContext;
    }
    if(!value::truthy(value::property(gadgetPermissionContext,
                               "isBypassPermissionsModeAvailable"))){
      services.enqueue(errorResponse(requestId,
        "Cannot set permission mode to bypassPermissions because the session was not launched with --dangerously-skip-permissions"));
      return gadgetPermissionContext;
    }
  }
  if(services.transcriptClassifierEnabled()&&mode=="auto"&&
     !services.isAutoModeGateEnabled()){
    const auto reason=services.getAutoModeUnavailableReason();
    services.enqueue(errorResponse(requestId,reason.empty()
      ?"Cannot set permission mode to auto"
      :"Cannot set permission mode to auto: "+
        services.getAutoModeUnavailableNotification(reason)));
    return gadgetPermissionContext;
  }
  services.enqueue(successResponse(requestId,mode));
  const auto& from=value::property(gadgetPermissionContext,"mode");
  if(!from.is_string()||!mode.is_string())
    throw std::invalid_argument("Permission mode must be a string");
  Json result=services.transitionPermissionMode(from.get<std::string>(),
                                                mode.get<std::string>(),
                                                gadgetPermissionContext);
  if(!result.is_object())
    throw std::invalid_argument("Permission transition must return an object");
  result["mode"]=mode;
  return result;
}
}
