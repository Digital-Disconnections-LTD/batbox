#pragma once
#include "instructions/hooks/runtime/EnvHooks.hpp"

namespace batbox::instructions::hooks::runtime {
event_loop::Deferred<event_loop::Unit> executeNotificationHooks(
  event_loop::Loop& loop,const EnvHookServices& services,
  const std::string& message,const std::optional<std::string>& title,
  const std::string& notificationType,
  std::optional<double> timeoutMs=std::nullopt);
} // namespace batbox::instructions::hooks::runtime
