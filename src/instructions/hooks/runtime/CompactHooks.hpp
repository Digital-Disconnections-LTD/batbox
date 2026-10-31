#pragma once
#include "instructions/hooks/runtime/EnvHooks.hpp"
#include <stop_token>

namespace batbox::instructions::hooks::runtime {
struct CompactHookOutput {
  std::optional<std::string> newCustomInstructions,userDisplayMessage;
  bool newCustomInstructionsOwnProperty=false,userDisplayMessageOwnProperty=false;
};
struct CompactHookServices {
  std::function<BaseHookInput()> createBaseHookInput;
  std::function<event_loop::Deferred<std::vector<HookResult>>(
    HookDispatch,std::optional<std::stop_token>)> executeHooksOutsideRepl;
  double defaultTimeoutMs=0;
};
event_loop::Deferred<CompactHookOutput> executePreCompactHooks(
  event_loop::Loop& loop,const CompactHookServices& services,
  const std::string& trigger,const std::optional<std::string>& customInstructions,
  std::optional<std::stop_token> signal=std::nullopt,
  std::optional<double> timeoutMs=std::nullopt);
event_loop::Deferred<CompactHookOutput> executePostCompactHooks(
  event_loop::Loop& loop,const CompactHookServices& services,
  const std::string& trigger,const std::string& compactSummary,
  std::optional<std::stop_token> signal=std::nullopt,
  std::optional<double> timeoutMs=std::nullopt);
} // namespace batbox::instructions::hooks::runtime
