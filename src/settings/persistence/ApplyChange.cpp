#include "settings/persistence/ApplyChange.hpp"
#include <stdexcept>

namespace batbox::settings::persistence {
namespace {
void need(bool okay,const char* name){
  if(!okay)throw std::invalid_argument(std::string("Missing settings-change binding: ")+name);
}
Json property(const Json& value,const char* key){
  return value.is_object()&&value.contains(key)?value[key]:Json(nullptr);
}
}
void applySettingsChange(Source source,const SetAppState& setAppState,
    const ApplyChangeServices& services){
  need(bool(services.getInitialSettings),"getInitialSettings");
  need(bool(services.debug),"debug");
  need(bool(services.loadAllPermissionRulesFromDisk),
    "loadAllPermissionRulesFromDisk");
  need(bool(services.updateHooksConfigSnapshot),"updateHooksConfigSnapshot");
  need(bool(setAppState),"setAppState");
  const auto newSettings=services.getInitialSettings();
  services.debug("Settings changed from "+sourceKey(source)+
    ", updating app state");
  const auto updatedRules=services.loadAllPermissionRulesFromDisk();
  services.updateHooksConfigSnapshot();
  setAppState([services,newSettings,updatedRules](const Json& prev){
    need(bool(services.syncPermissionRulesFromDisk),
      "syncPermissionRulesFromDisk");
    Json newContext=services.syncPermissionRulesFromDisk(
      prev.at("toolPermissionContext"),updatedRules);
    if(services.internalUser&&!services.localBatEntrypoint){
      need(bool(services.findOverlyBroadBashPermissions),
        "findOverlyBroadBashPermissions");
      const auto overlyBroad=services.findOverlyBroadBashPermissions(
        updatedRules,Json::array());
      if(overlyBroad.is_array()&&!overlyBroad.empty()){
        need(bool(services.removeDangerousPermissions),
          "removeDangerousPermissions");
        newContext=services.removeDangerousPermissions(newContext,overlyBroad);
      }
    }
    if(value::truthy(property(newContext,"isBypassPermissionsModeAvailable"))){
      need(bool(services.isBypassPermissionsModeDisabled),
        "isBypassPermissionsModeDisabled");
      if(services.isBypassPermissionsModeDisabled()){
        need(bool(services.createDisabledBypassPermissionsContext),
          "createDisabledBypassPermissionsContext");
        newContext=services.createDisabledBypassPermissionsContext(newContext);
      }
    }
    need(bool(services.transitionPlanAutoMode),"transitionPlanAutoMode");
    newContext=services.transitionPlanAutoMode(newContext);
    const auto& prevSettings=prev.at("settings");
    const bool prevDefined=prevSettings.is_object()&&
      prevSettings.contains("effortLevel");
    const bool nextDefined=newSettings.is_object()&&
      newSettings.contains("effortLevel");
    const bool effortChanged=prevDefined!=nextDefined||
      (prevDefined&&prevSettings["effortLevel"]!=newSettings["effortLevel"]);
    Json result=prev;
    result["settings"]=newSettings;
    result["toolPermissionContext"]=newContext;
    if(effortChanged&&nextDefined)
      result["effortValue"]=newSettings["effortLevel"];
    return result;
  });
}
}
