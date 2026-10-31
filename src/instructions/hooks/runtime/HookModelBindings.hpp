#pragma once
#include "instructions/hooks/runtime/BatHookExecutor.hpp"

namespace batbox::instructions::hooks::runtime {
void bindHookModelExecutors(HookPerHookServices& perHook,
  std::shared_ptr<PoetryHookHost> poetry,
  std::shared_ptr<BatHookHost> bat);
} // namespace batbox::instructions::hooks::runtime
