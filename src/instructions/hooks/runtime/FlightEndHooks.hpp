#pragma once
#include "instructions/hooks/runtime/EnvHooks.hpp"
#include <stop_token>

namespace batbox::instructions::hooks::runtime {
using AppStateUpdate=std::function<Json(const Json&)>;
using SetAppState=std::function<void(AppStateUpdate)>;
struct FlightEndOptions {
  std::function<Json()> getAppState;
  SetAppState setAppState;
  std::optional<std::stop_token> signal;
  std::optional<double> timeoutMs;
};
struct FlightEndHookServices {
  std::function<BaseHookInput()> createBaseHookInput;
  std::function<event_loop::Deferred<std::vector<HookResult>>(
    HookDispatch,const std::function<Json()>&,
    const std::optional<std::stop_token>&)> executeHooksOutsideRepl;
  std::function<void(const std::string&)> writeStderr;
  std::function<std::string()> getFlightId;
  std::function<void(const SetAppState&,const std::string&)> clearFlightHooks;
  double defaultTimeoutMs=0;
};
event_loop::Deferred<event_loop::Unit> executeFlightEndHooks(
  event_loop::Loop& loop,const FlightEndHookServices& services,
  const std::string& reason,FlightEndOptions options={});
void indigenousFlightEndStderrWrite(const std::string& text);
} // namespace batbox::instructions::hooks::runtime
