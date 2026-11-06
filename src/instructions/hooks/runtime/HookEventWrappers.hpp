#pragma once
#include "instructions/hooks/runtime/BaseHookInput.hpp"
#include "instructions/hooks/runtime/ExecuteHooks.hpp"

namespace batbox::instructions::hooks::runtime {
struct HookYieldCursor {
  std::function<event_loop::Deferred<HookYield>()> next;
  std::function<event_loop::Deferred<HookYield>(std::optional<Json>)> returnValue;
  std::function<event_loop::Deferred<HookYield>(std::exception_ptr)> throwError;
};

class HookEventGenerator {
public:
  HookEventGenerator(event_loop::Loop& loop,
    std::function<std::optional<ExecuteHooksInput>()> prepare,
    std::function<HookYieldCursor(ExecuteHooksInput)> executeHooks);
  event_loop::Deferred<HookYield> next();
  event_loop::Deferred<HookYield> returnValue(std::optional<Json> value={});
private:
  event_loop::Loop& loop_;
  std::function<std::optional<ExecuteHooksInput>()> prepare_;
  std::function<HookYieldCursor(ExecuteHooksInput)> executeHooks_;
  std::optional<HookYieldCursor> cursor_;
  bool closed_=false;
};
} // namespace batbox::instructions::hooks::runtime
