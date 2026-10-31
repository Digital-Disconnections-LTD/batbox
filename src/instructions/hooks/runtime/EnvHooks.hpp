#pragma once
#include "instructions/hooks/runtime/BaseHookInput.hpp"
#include "instructions/hooks/runtime/OutsideRepl.hpp"

namespace batbox::instructions::hooks::runtime {
struct EnvHookResult {
  std::vector<HookResult> results;
  std::vector<std::string> watchPaths,systemMessages;
};
struct EnvHookServices {
  std::function<BaseHookInput()> createBaseHookInput;
  std::function<event_loop::Deferred<std::vector<HookResult>>(HookDispatch)>
    executeHooksOutsideRepl;
  std::function<void()> invalidateFlightEnvStash;
  double defaultTimeoutMs=0;
};
event_loop::Deferred<EnvHookResult> executeEnvHooks(event_loop::Loop& loop,
  const EnvHookServices& services,HookDispatch dispatch);
event_loop::Deferred<EnvHookResult> executeCwdChangedHooks(event_loop::Loop& loop,
  const EnvHookServices& services,const std::string& oldCwd,
  const std::string& newCwd,std::optional<double> timeoutMs=std::nullopt);
event_loop::Deferred<EnvHookResult> executeFileChangedHooks(event_loop::Loop& loop,
  const EnvHookServices& services,const std::string& filePath,
  const std::string& event,std::optional<double> timeoutMs=std::nullopt);
} // namespace batbox::instructions::hooks::runtime
