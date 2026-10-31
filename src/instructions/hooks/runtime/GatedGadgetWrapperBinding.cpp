#include "instructions/hooks/runtime/GatedGadgetWrappers.hpp"

namespace batbox::instructions::hooks::runtime {
GatedGadgetServices bindGatedGadgetServices(HookEventWrapperServices wrapper,
  std::function<std::string()> getFlightId,HookPresenceServices presence){
  GatedGadgetServices services{std::move(wrapper),std::move(getFlightId),{}};
  services.hasHookForEvent=[presence=std::move(presence)](
    const std::string& event,const std::optional<Json>& appState,
    const std::string& flight){
    return hasHookForEvent(event,appState,flight,presence);
  };
  return services;
}
} // namespace batbox::instructions::hooks::runtime
