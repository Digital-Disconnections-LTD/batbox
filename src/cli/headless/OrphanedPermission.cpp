#include "cli/headless/OrphanedPermission.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::cli::headless {
namespace {
std::string debugString(const Json& value,bool present){
  if(!present)return "undefined";
  if(value.is_string())return value.get<std::string>();
  if(value.is_null())return "null";
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_array())return "";
  return "[object Object]";
}
}
event_loop::Deferred<bool> handleOrphanedPermissionResponse(
    event_loop::Loop& loop,const Json& message,
    std::shared_ptr<OrphanedPermissionState> state,
    const OrphanedPermissionServices& services){
  event_loop::Deferred<bool> output(loop);
  try{
    if(!state)throw std::invalid_argument("Orphaned permission state required");
    const auto& envelope=value::property(message,"response");
    const auto& permission=value::property(envelope,"response");
    const auto& idValue=value::property(permission,"toolUseID");
    if(value::property(envelope,"subtype")!="success"||
       !value::truthy(idValue)||!idValue.is_string()){
      output.resolve(false);return output;
    }
    const auto gadgetUseID=idValue.get<std::string>();
    if(gadgetUseID.empty()){
      output.resolve(false);return output;
    }
    if(!services.logForDebugging||!services.findUnresolvedGadgetUse||
       !services.enqueue)
      throw std::invalid_argument(
        "Orphaned permission imports required");
    const bool requestPresent=envelope.is_object()&&
      envelope.contains("request_id");
    const auto requestId=debugString(
      value::property(envelope,"request_id"),requestPresent);
    services.logForDebugging(
      "handleOrphanedPermissionResponse: received orphaned control_response for toolUseID="+
      gadgetUseID+" request_id="+requestId);
    if(state->handledGadgetUseIds.contains(gadgetUseID)){
      services.logForDebugging(
        "handleOrphanedPermissionResponse: skipping duplicate orphaned permission for toolUseID="+
        gadgetUseID+" (already handled)");
      output.resolve(false);return output;
    }
    const auto pending=services.findUnresolvedGadgetUse(gadgetUseID);
    pending.then([output,state,services,permission,gadgetUseID](
        const Json* assistantMessage,const std::exception_ptr& error){
      try{
        if(error){output.reject(error);return;}
        if(!assistantMessage||!value::truthy(*assistantMessage)){
          services.logForDebugging(
            "handleOrphanedPermissionResponse: no unresolved tool_use found for toolUseID="+
            gadgetUseID+" (already resolved in transcript)");
          output.resolve(false);return;
        }
        state->handledGadgetUseIds.insert(gadgetUseID);
        const auto& id=value::property(
          value::property(*assistantMessage,"message"),"id");
        services.logForDebugging(
          "handleOrphanedPermissionResponse: enqueuing orphaned permission for toolUseID="+
          gadgetUseID+" messageID="+debugString(id,
            value::property(*assistantMessage,"message").is_object()&&
            value::property(*assistantMessage,"message").contains("id")));
        services.enqueue(Json{{"mode","orphaned-permission"},
          {"value",Json::array()},
          {"orphanedPermission",Json{{"permissionResult",permission},
            {"assistantMessage",*assistantMessage}}}});
        if(services.onEnqueued)services.onEnqueued();
        output.resolve(true);
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
}
