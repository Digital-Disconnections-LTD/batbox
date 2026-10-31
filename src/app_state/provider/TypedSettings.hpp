#pragma once
#include "app_state/Host.hpp"
#include "settings/Sources.hpp"

namespace batbox::app_state::provider {
struct SettingsRecord {
  value::ValueRef value;
  value::ValueRef env;
};
struct TypedSettingsServices {
  std::function<SettingsRecord()> getInitialSettings;
  std::function<void(const std::string&)> debug;
  std::function<Json()> loadAllPermissionRulesFromDisk;
  std::function<void()> updateHooksConfigSnapshot;
  std::function<value::RecordRef(value::RecordRef,const Json&)>
    syncPermissionRulesFromDisk;
  std::function<Json(const Json&,const Json&)>
    findOverlyBroadBashPermissions;
  std::function<value::RecordRef(value::RecordRef,const Json&)>
    removeDangerousPermissions;
  std::function<bool()> isBypassPermissionsModeDisabled;
  std::function<value::RecordRef(value::RecordRef)>
    createDisabledBypassPermissionsContext,transitionPlanAutoMode;
  bool internalUser=false;
  bool localBatEntrypoint=false;
};
void applySettingsChangeToHost(settings::Source source,Host& host,
  const TypedSettingsServices& services);
} // namespace batbox::app_state::provider
