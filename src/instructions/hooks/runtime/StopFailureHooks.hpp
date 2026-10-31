#pragma once
#include "instructions/hooks/runtime/BaseHookInput.hpp"
#include "instructions/hooks/runtime/OutsideRepl.hpp"

namespace batbox::instructions::hooks::runtime {
struct StopFailureMessage {
  Json content=Json::array();
  std::optional<std::string> error;
  std::optional<Json> errorDetails;
};
struct StopFailureContext {
  std::function<Json()> getAppState;
  std::optional<std::string> batId,batType;
};
struct HookPresenceServices {
  std::function<std::optional<Json>()> getHooksConfigFromSnapshot,
    getRegisteredHooks;
  // Keep flightHooks lookup on the typed application state so the Map retains
  // its callback identities.
  std::function<bool(const std::optional<Json>&,const std::string&,
    const std::string&)> hasFlightHook;
};
bool hasHookForEvent(const std::string& event,
  const std::optional<Json>& appState,const std::string& flightId,
  const HookPresenceServices& services);
struct StopFailureServices {
  HookPresenceServices presence;
  std::function<std::string()> getFlightId;
  std::function<BaseHookInput(const StopFailureContext*)> createBaseHookInput;
  std::function<event_loop::Deferred<std::vector<HookResult>>(
    HookDispatch,const std::function<Json()>&)> executeHooksOutsideRepl;
  double defaultTimeoutMs=0;
};
event_loop::Deferred<event_loop::Unit> executeStopFailureHooks(
  event_loop::Loop& loop,const StopFailureServices& services,
  const StopFailureMessage& lastMessage,
  const StopFailureContext* context=nullptr,
  std::optional<double> timeoutMs=std::nullopt);
} // namespace batbox::instructions::hooks::runtime
