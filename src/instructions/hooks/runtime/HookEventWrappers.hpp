#pragma once
#include "instructions/hooks/runtime/BaseHookInput.hpp"
#include "instructions/hooks/runtime/ExecuteHooks.hpp"

namespace batbox::instructions::hooks::runtime {
struct HookYieldCursor {
  std::function<event_loop::Deferred<HookYield>()> next;
  std::function<event_loop::Deferred<HookYield>(std::optional<Json>)> returnValue;
  std::function<event_loop::Deferred<HookYield>(std::exception_ptr)> throwError;
};
struct HookEventWrapperServices {
  std::function<Json(std::optional<std::string>,
    std::optional<std::string>,std::optional<HookBatInfo>)>
    createBaseHookInput;
  std::function<std::string()> randomUUID;
  std::function<HookYieldCursor(ExecuteHooksInput)> executeHooks;
};
HookEventWrapperServices bindHookEventWrapperServices(event_loop::Loop& loop,
  BaseHookInputServices base,std::function<std::string()> randomUUID,
  ExecuteHooksServices executor);

class HookEventGenerator {
public:
  HookEventGenerator(event_loop::Loop& loop,
    std::function<std::optional<ExecuteHooksInput>()> prepare,
    std::function<HookYieldCursor(ExecuteHooksInput)> executeHooks);
  event_loop::Deferred<HookYield> next();
  event_loop::Deferred<HookYield> returnValue(std::optional<Json> value={});
  event_loop::Deferred<HookYield> throwError(std::exception_ptr error);
private:
  event_loop::Loop& loop_;
  std::function<std::optional<ExecuteHooksInput>()> prepare_;
  std::function<HookYieldCursor(ExecuteHooksInput)> executeHooks_;
  std::optional<HookYieldCursor> cursor_;
  bool closed_=false;
};

struct FlightStartHookCall {
  std::string source;
  std::optional<std::string> flightId,batType,model;
  std::optional<std::stop_token> signal;
  double timeoutMs=600000;
  bool forceSyncExecution=false;
};
struct SetupHookCall {
  std::string trigger;
  std::optional<std::stop_token> signal;
  double timeoutMs=600000;
  bool forceSyncExecution=false;
};
struct SubagentStartHookCall {
  std::string batId,batType;
  std::optional<std::stop_token> signal;
  double timeoutMs=600000;
};
HookEventGenerator executeFlightStartHooks(event_loop::Loop& loop,
  FlightStartHookCall call,HookEventWrapperServices services);
HookEventGenerator executeSetupHooks(event_loop::Loop& loop,
  SetupHookCall call,HookEventWrapperServices services);
HookEventGenerator executeSubagentStartHooks(event_loop::Loop& loop,
  SubagentStartHookCall call,HookEventWrapperServices services);
} // namespace batbox::instructions::hooks::runtime
