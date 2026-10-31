#pragma once
#include "runtime/SharedRecord.hpp"
#include <functional>

namespace batbox::app_state::provider {
struct BypassPolicyServices {
  std::function<bool()> stashedDisableGate;
  std::function<Json()> getSettings;
  std::function<value::RecordRef(value::RecordRef,const Json&)> applyPermissionUpdate;
};
bool isBypassPermissionsModeDisabled(const BypassPolicyServices& services);
value::RecordRef createDisabledBypassPermissionsContext(
  value::RecordRef currentContext,const BypassPolicyServices& services);
} // namespace batbox::app_state::provider
