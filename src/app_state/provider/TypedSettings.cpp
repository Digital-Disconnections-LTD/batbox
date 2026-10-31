#include "app_state/provider/TypedSettings.hpp"
#include <stdexcept>

namespace batbox::app_state::provider {
namespace {
void required(bool present,const char* name){
  if(!present)throw std::invalid_argument(std::string("Settings change import required: ")+name);
}
Json value(const value::ValueRef& ref,const char* key){
  return ref&&ref->is_object()&&ref->contains(key)?ref->at(key):Json(nullptr);
}
bool has(const value::ValueRef& ref,const char* key){
  return ref&&ref->is_object()&&ref->contains(key);
}
bool effortChanged(const value::ValueRef& oldSettings,
    const value::ValueRef& newSettings){
  const bool oldHas=has(oldSettings,"effortLevel");
  const bool newHas=has(newSettings,"effortLevel");
  return oldHas!=newHas||(oldHas&&
    value(oldSettings,"effortLevel")!=value(newSettings,"effortLevel"));
}
}
void applySettingsChangeToHost(settings::Source source,Host& host,
    const TypedSettingsServices& services){
  required(bool(services.getInitialSettings),"getInitialSettings");
  required(bool(services.debug),"logForDebugging");
  required(bool(services.loadAllPermissionRulesFromDisk),
    "loadAllPermissionRulesFromDisk");
  required(bool(services.updateHooksConfigSnapshot),
    "updateHooksConfigSnapshot");
  const SettingsRecord newSettings=services.getInitialSettings();
  required(bool(newSettings.value),"settings object identity");
  services.debug("Settings changed from "+settings::sourceKey(source)+
    ", updating app state");
  const Json updatedRules=services.loadAllPermissionRulesFromDisk();
  services.updateHooksConfigSnapshot();

  host.setState([services,newSettings,updatedRules](const AppStateRef& prev){
    required(bool(services.syncPermissionRulesFromDisk),
      "syncPermissionRulesFromDisk");
    auto context=services.syncPermissionRulesFromDisk(
      prev->permissionContext,updatedRules);
    required(bool(context),"updated permission context");
    if(services.internalUser&&!services.localBatEntrypoint){
      required(bool(services.findOverlyBroadBashPermissions),
        "findOverlyBroadBashPermissions");
      const auto broad=services.findOverlyBroadBashPermissions(
        updatedRules,Json::array());
      if(broad.is_array()&&!broad.empty()){
        required(bool(services.removeDangerousPermissions),
          "removeDangerousPermissions");
        context=services.removeDangerousPermissions(context,broad);
        required(bool(context),"stripped permission context");
      }
    }
    const auto available=context->get("isBypassPermissionsModeAvailable");
    if(available&&value::truthy(*available)){
      required(bool(services.isBypassPermissionsModeDisabled),
        "isBypassPermissionsModeDisabled");
      if(services.isBypassPermissionsModeDisabled()){
        required(bool(services.createDisabledBypassPermissionsContext),
          "createDisabledBypassPermissionsContext");
        context=services.createDisabledBypassPermissionsContext(context);
        required(bool(context),"disabled permission context");
      }
    }
    required(bool(services.transitionPlanAutoMode),
      "transitionPlanAutoMode");
    context=services.transitionPlanAutoMode(context);
    required(bool(context),"transitioned permission context");
    const auto oldSettings=prev->fieldRef("settings");
    const bool changed=effortChanged(oldSettings,newSettings.value);
    auto next=std::make_shared<AppState>(prev->shallowCopy());
    next->setSettings(newSettings.value,newSettings.env);
    next->setPermissionContext(context);
    if(changed&&has(newSettings.value,"effortLevel"))
      next->fields->set("effortValue",
        value(newSettings.value,"effortLevel"));
    return next;
  });
}
} // namespace batbox::app_state::provider
