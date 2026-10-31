#include "app_state/OnChange.hpp"
#include <stdexcept>

namespace batbox::app_state {
AppStateUpdater externalMetadataToAppState(const Json& metadata,
  std::function<std::string(const std::string&)> permissionModeFromString){
  return [metadata,permissionModeFromString=std::move(permissionModeFromString)](
    const AppStateRef& previous){
    auto next=std::make_shared<AppState>(previous->shallowCopy());
    const auto& mode=value::property(metadata,"permission_mode");
    if(mode.is_string()){
      if(!permissionModeFromString)
        throw std::invalid_argument("Permission mode parser required");
      if(!previous->permissionContext)
        throw std::invalid_argument("Permission context record required");
      auto context=std::make_shared<value::SharedRecord>(
        previous->permissionContext->shallowCopy());
      context->set("mode",permissionModeFromString(mode.get<std::string>()));
      next->setPermissionContext(std::move(context));
    }
    const auto& ultramode=value::property(metadata,"is_ultraplan_mode");
    if(ultramode.is_boolean())next->fields->set("isUltraplanMode",ultramode);
    return next;
  };
}
} // namespace batbox::app_state
