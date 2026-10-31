#pragma once
#include "instructions/hooks/runtime/AsyncHookRoster.hpp"
#include "instructions/hooks/runtime/CompactHooks.hpp"
#include "instructions/hooks/runtime/ElicitationHooks.hpp"
#include "instructions/hooks/runtime/HookConfig.hpp"
#include "instructions/hooks/runtime/IndigenousCommand.hpp"
#include "instructions/hooks/runtime/FlightEndHooks.hpp"
#include "instructions/hooks/runtime/FlightEnvironment.hpp"
#include "instructions/hooks/runtime/StopFailureHooks.hpp"
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
FlightEndHookServices bindFlightEndHookServices(event_loop::Loop& loop,
  BaseHookInputServices base,ExecutorServices executor,
  std::function<void(const SetAppState&,const std::string&)> clearFlightHooks,
  double defaultTimeoutMs);
StopFailureServices bindStopFailureHookServices(event_loop::Loop& loop,
  BaseHookInputServices base,ExecutorServices executor,
  HookConfigServices config,double defaultTimeoutMs);
WorktreeHookServices bindWorktreeHookServices(event_loop::Loop& loop,
  WorktreeHookServices configAndDiagnostics,BaseHookInputServices base,
  ExecutorServices executor,double defaultTimeoutMs);
ElicitationHookServices bindElicitationHookServices(event_loop::Loop& loop,
  BaseHookInputServices base,ExecutorServices executor,
  HookSchemaImports schema,double defaultTimeoutMs);
void bindHookFlightEnvironment(IndigenousCommandConfig& command,
  AsyncHookServices& roster,std::shared_ptr<FlightEnvironment> environment);
} // namespace batbox::instructions::hooks::runtime
