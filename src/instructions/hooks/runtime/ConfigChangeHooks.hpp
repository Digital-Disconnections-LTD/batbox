#pragma once
#include "instructions/hooks/runtime/EnvHooks.hpp"

namespace batbox::instructions::hooks::runtime {
event_loop::Deferred<std::vector<HookResult>> executeConfigChangeHooks(
  event_loop::Loop& loop,const EnvHookServices& services,
  const std::string& source,std::optional<std::string> filePath=std::nullopt,
  std::optional<double> timeoutMs=std::nullopt);
} // namespace batbox::instructions::hooks::runtime
