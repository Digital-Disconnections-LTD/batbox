#pragma once
#include "app_state/State.hpp"
#include <exception>

namespace batbox::app_state {
struct AppStateChangeServices {
  std::function<std::string(const std::string&)> toExternalPermissionMode,
    permissionModeFromString;
  std::function<void(value::RecordRef)> notifyFlightMetadataChanged;
  std::function<void(const std::string&)> notifyPermissionModeChanged;
  std::function<void(const std::string&,value::RecordRef)>
    updateSettingsForSource;
  std::function<void(const Json&)> setMainLoopModelOverride;
  std::function<Json()> getGlobalConfig;
  std::function<void(const std::function<Json(const Json&)>&)>
    saveGlobalConfig;
  std::function<bool()> isPrivilegedUserType;
  std::function<void()> clearApiKeyHelperStash,clearAwsCredentialsStash,
    clearGcpCredentialsStash,applyConfigEnvironmentVariables;
  std::function<std::exception_ptr(std::exception_ptr)> toError;
  std::function<void(std::exception_ptr)> logError;
};
using AppStateUpdater=std::function<AppStateRef(const AppStateRef&)>;
AppStateUpdater externalMetadataToAppState(const Json& metadata,
  std::function<std::string(const std::string&)> permissionModeFromString);
void onChangeAppState(const AppStateRef& newState,
  const AppStateRef& oldState,const AppStateChangeServices& services);
} // namespace batbox::app_state
