#include "instructions/hooks/runtime/HookConfig.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
std::vector<HookMatcher> getHooksConfig(
  const std::optional<Json>& appState,const std::string& flightId,
  const std::string& hookEvent,const HookConfigServices& services){
  if(!services.getSnapshotHooks||!services.getRegisteredHooks||
    !services.managedOnly)
    throw std::invalid_argument("Hook config providers required");
  auto hooks=services.getSnapshotHooks(hookEvent).value_or(
    std::vector<HookMatcher>{});
  const bool managedOnly=services.managedOnly();
  const auto registered=services.getRegisteredHooks(hookEvent);
  if(registered)for(const auto& matcher:*registered){
    if(managedOnly&&matcher.pluginRoot)continue;
    hooks.push_back(matcher);
  }
  if(!managedOnly&&appState){
    if(!services.getFlightHooks||!services.getFlightFunctionHooks)
      throw std::invalid_argument("Hook session providers required");
    const auto flight=services.getFlightHooks(*appState,flightId,hookEvent);
    if(flight)for(const auto& matcher:*flight)hooks.push_back(matcher);
    const auto functions=services.getFlightFunctionHooks(*appState,
      flightId,hookEvent);
    if(functions)for(const auto& matcher:*functions)hooks.push_back(matcher);
  }
  return hooks;
}
void bindHookConfig(HookMatchServices& matcher,HookConfigServices services){
  matcher.getHooksConfig=[services=std::move(services)](const std::optional<Json>& appState,
    const std::string& flight,const std::string& event){
    return getHooksConfig(appState,flight,event,services);
  };
}
} // namespace batbox::instructions::hooks::runtime
