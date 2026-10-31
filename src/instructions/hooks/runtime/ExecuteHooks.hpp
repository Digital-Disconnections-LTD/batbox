#pragma once
#include "instructions/hooks/runtime/HookBatchStart.hpp"
#include "instructions/hooks/runtime/HookPerHook.hpp"

namespace batbox::instructions::hooks::runtime {
struct ExecuteHooksInput {
  HookBatchStartInput batch;
  std::optional<Json> messages;
  std::optional<double> timeoutMs;
  bool forceSyncExecution=false;
};
struct ExecuteHooksServices {
  HookBatchStartServices batch;
  HookPerHookServices perHook;
  HookResultLoopServices results;
  std::function<double()> defaultTimeoutMs;
  std::function<std::string(const Json&)> jsonStringify;
  std::function<void(const std::string&,std::exception_ptr)> logError;
};
class ExecuteHooks {
public:
  ExecuteHooks(event_loop::Loop& loop,ExecuteHooksInput input,
    ExecuteHooksServices services);
  event_loop::Deferred<HookYield> next();
  event_loop::Deferred<HookYield> returnValue(std::optional<Json> value={});
  event_loop::Deferred<HookYield> throwError(std::exception_ptr error);
private:
  struct State;
  std::shared_ptr<State> state_;
};
} // namespace batbox::instructions::hooks::runtime
