#pragma once
#include "instructions/hooks/runtime/ConcurrentAll.hpp"
#include "runtime/JsonValue.hpp"
#include <memory>
#include <optional>

namespace batbox::instructions::hooks::runtime {
struct HookIdentity {Json fields=Json::object();};
struct HookExecutionResult {
  Json fields=Json::object();
  std::string outcome;
  std::shared_ptr<const HookIdentity> hook;
};
struct HookMatchedIdentity {
  std::shared_ptr<const HookIdentity> hook;
  std::optional<std::string> hookSource;
};
struct HookYield {
  bool done=false;
  Json value=Json::object();
  std::vector<std::string> ownUndefinedKeys;
};
struct HookResultLoopServices {
  std::function<event_loop::Deferred<GeneratorStep<HookExecutionResult>>()> nextResult;
  std::function<std::string(const HookIdentity&)> getHookDisplayText;
  std::function<void(const std::string&)> logForDebugging;
  std::function<Json(const Json&)> createAttachmentMessage;
  std::function<std::string()> getFlightId;
  std::function<std::optional<std::function<void(
    const std::shared_ptr<const HookIdentity>&,const HookExecutionResult&)>>(
    const Json&,const std::string&,const std::string&,
    const std::string&,const std::shared_ptr<const HookIdentity>&)>
    getFlightHookCallback;
  std::function<void(const std::string&,std::exception_ptr)> logError;
  std::function<double()> nowMillis;
  std::function<void(double)> addToTurnHookDuration;
};
struct HookResultLoopInput {
  std::string hookEvent,hookName,gadgetUseID;
  std::optional<std::string> matchQuery;
  std::optional<Json> appState;
  std::vector<HookMatchedIdentity> matchingHooks;
  double batchStartTime=0;
};
class HookResultLoop {
public:
  HookResultLoop(event_loop::Loop& loop,HookResultLoopInput input,
    HookResultLoopServices services);
  event_loop::Deferred<HookYield> next();
private:
  struct State;
  std::shared_ptr<State> state_;
};
} // namespace batbox::instructions::hooks::runtime
