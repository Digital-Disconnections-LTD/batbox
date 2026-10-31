#pragma once
#include "runtime/JsonValue.hpp"
#include "settings/Sources.hpp"
#include <functional>
#include <string>

namespace batbox::settings::persistence {
struct ApplyChangeServices {
  std::function<Json()> getInitialSettings;
  std::function<void(const std::string&)> debug;
  std::function<Json()> loadAllPermissionRulesFromDisk;
  std::function<void()> updateHooksConfigSnapshot;
  std::function<Json(const Json&,const Json&)> syncPermissionRulesFromDisk;
  std::function<Json(const Json&,const Json&)> findOverlyBroadBashPermissions;
  std::function<Json(const Json&,const Json&)> removeDangerousPermissions;
  std::function<bool()> isBypassPermissionsModeDisabled;
  std::function<Json(const Json&)> createDisabledBypassPermissionsContext;
  std::function<Json(const Json&)> transitionPlanAutoMode;
  bool internalUser=false;
  bool localBatEntrypoint=false;
};
using SetAppState=std::function<void(std::function<Json(const Json&)>)>;
void applySettingsChange(Source source,const SetAppState& setAppState,
  const ApplyChangeServices& services);
}
