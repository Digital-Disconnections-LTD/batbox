#pragma once
#include "instructions/hooks/runtime/HookMatching.hpp"

namespace batbox::instructions::hooks::runtime {
struct HookConfigServices {
  std::function<std::optional<std::vector<HookMatcher>>(const std::string&)>
    getSnapshotHooks,getRegisteredHooks;
  std::function<bool()> managedOnly;
  std::function<std::optional<std::vector<HookMatcher>>(
    const Json&,const std::string&,const std::string&)>
    getFlightHooks,getFlightFunctionHooks;
};
std::vector<HookMatcher> getHooksConfig(
  const std::optional<Json>& appState,const std::string& flightId,
  const std::string& hookEvent,const HookConfigServices& services);
void bindHookConfig(HookMatchServices& matcher,HookConfigServices services);
} // namespace batbox::instructions::hooks::runtime
