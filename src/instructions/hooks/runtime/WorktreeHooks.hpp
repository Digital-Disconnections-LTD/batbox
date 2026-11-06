#pragma once
#include "instructions/hooks/runtime/BaseHookInput.hpp"
#include "instructions/hooks/runtime/OutsideRepl.hpp"

namespace batbox::instructions::hooks::runtime {
struct WorktreeHookServices {
  std::function<BaseHookInput()> createBaseHookInput;
  std::function<event_loop::Deferred<std::vector<HookResult>>(HookDispatch)>
    executeHooksOutsideRepl;
  std::function<void(const std::string&,const std::string&)> logForDebugging;
  double defaultTimeoutMs=0;
};
event_loop::Deferred<std::string> executeWorktreeCreateHook(
  event_loop::Loop& loop,const WorktreeHookServices& services,
  const std::string& name);
} // namespace batbox::instructions::hooks::runtime
