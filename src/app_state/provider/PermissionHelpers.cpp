#include "app_state/provider/PermissionHelpers.hpp"
#include <stdexcept>

namespace batbox::app_state::provider {
bool isBypassPermissionsModeDisabled(const BypassPolicyServices& services){
  if(!services.stashedDisableGate||!services.getSettings)
    throw std::invalid_argument("Bypass policy gate and settings imports required");
  const bool gate=services.stashedDisableGate();
  const Json settings=services.getSettings();
  bool setting=false;
  if(settings.is_object()&&settings.contains("permissions")){
    const auto& permissions=settings.at("permissions");
    setting=permissions.is_object()&&
      permissions.value("disableBypassPermissionsMode",Json(nullptr))=="disable";
  }
  return gate||setting;
}
value::RecordRef createDisabledBypassPermissionsContext(
    value::RecordRef currentContext,const BypassPolicyServices& services){
  if(!currentContext)throw std::invalid_argument("Permission context required");
  auto updated=currentContext;
  const auto mode=currentContext->get("mode");
  if(mode&&*mode=="bypassPermissions"){
    if(!services.applyPermissionUpdate)
      throw std::invalid_argument("Permission update import required");
    updated=services.applyPermissionUpdate(currentContext,
      Json{{"type","setMode"},{"mode","default"},
        {"destination","session"}});
    if(!updated)throw std::invalid_argument("Permission update returned no context");
  }
  auto result=std::make_shared<value::SharedRecord>(updated->shallowCopy());
  result->set("isBypassPermissionsModeAvailable",false);
  return result;
}
} // namespace batbox::app_state::provider
