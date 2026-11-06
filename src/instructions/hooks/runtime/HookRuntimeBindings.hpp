#pragma once
#include "instructions/hooks/runtime/AsyncHookRoster.hpp"
#include "instructions/hooks/runtime/CompactHooks.hpp"
#include "instructions/hooks/runtime/ElicitationHooks.hpp"
#include "instructions/hooks/runtime/HookConfig.hpp"
#include "instructions/hooks/runtime/FlightEnvironment.hpp"
#include "instructions/hooks/runtime/WorktreeHooks.hpp"
#include <memory>

namespace batbox::instructions::hooks::runtime {
EnvHookServices bindEnvHookServices(event_loop::Loop& loop,
  BaseHookInputServices base,ExecutorServices executor,
  std::shared_ptr<FlightEnvironment> flightEnvironment,
  double defaultTimeoutMs);
EnvHookServices bindNotificationHookServices(event_loop::Loop& loop,
  BaseHookInputServices base,ExecutorServices executor,
  double defaultTimeoutMs);
CompactHookServices bindCompactHookServices(event_loop::Loop& loop,
  BaseHookInputServices base,ExecutorServices executor,double defaultTimeoutMs);
WorktreeHookServices bindWorktreeHookServices(event_loop::Loop& loop,
  WorktreeHookServices configAndDiagnostics,BaseHookInputServices base,
  ExecutorServices executor,double defaultTimeoutMs);
ElicitationHookServices bindElicitationHookServices(event_loop::Loop& loop,
  BaseHookInputServices base,ExecutorServices executor,
  HookSchemaImports schema,double defaultTimeoutMs);
} // namespace batbox::instructions::hooks::runtime
