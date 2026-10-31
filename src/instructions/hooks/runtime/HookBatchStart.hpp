#pragma once
#include "instructions/hooks/runtime/HookCallback.hpp"
#include "instructions/hooks/runtime/FunctionHook.hpp"
#include "instructions/hooks/runtime/HookResultLoop.hpp"
#include <deque>

namespace batbox::instructions::hooks::runtime {
struct HookBatchMatch {
  std::shared_ptr<const HookIdentity> hook;
  std::shared_ptr<const HookCallback> callback;
  std::shared_ptr<const FunctionHook> function;
  std::optional<std::string> pluginRoot,pluginId,skillRoot,hookSource;
};
struct HookBatchContext {
  std::function<Json()> getAppState;
  std::function<void(const Json&)> updateAttributionState;
  std::optional<std::string> batId,batType;
  std::optional<Json> gadgets;
  std::shared_ptr<void> sourceContext;
};
using BoundHookPoetry=std::function<event_loop::Deferred<Json>(const Json&)>;
struct HookBatchStartInput {
  Json hookInput=Json::object();
  std::string gadgetUseID;
  std::optional<std::string> matchQuery;
  std::optional<Json> gadgetInputSummary;
  std::optional<std::stop_token> signal;
  std::optional<HookBatchContext> gadgetUseContext;
};
struct HookBatchStartStep {
  enum class Kind { Done,Progress,Ready } kind=Kind::Done;
  Json progress=Json(nullptr);
  std::vector<std::string> ownUndefinedKeys;
  std::vector<HookBatchMatch> matches;
  std::optional<Json> appState;
  std::string flightId,hookEvent,hookName;
  double batchStartTime=0;
  BoundHookPoetry boundRequestPoetry;
};
struct HookBatchStartServices {
  std::function<bool()> shouldDisableAllHooksIncludingManaged,
    isSimpleEnvironment,shouldSkipHookDueToTrust;
  std::function<void(const std::string&)> logForDebugging;
  std::function<std::string()> getFlightId,randomUUID,nowIso;
  std::function<double()> nowMillis;
  std::function<void(double)> addToTurnHookDuration;
  std::function<bool(const HookBatchMatch&)> isInternalHook;
  std::function<std::string(const HookBatchMatch&)> getHookDisplayText;
  std::function<event_loop::Deferred<std::vector<HookBatchMatch>>(
    const std::optional<Json>&,const std::string&,const std::string&,
    const Json&,const std::optional<Json>&)> getMatchingHooks;
  std::function<BoundHookPoetry(const std::string&,
    const std::optional<Json>&)> bindRequestPoetry;
};
class HookBatchStart {
public:
  HookBatchStart(event_loop::Loop& loop,HookBatchStartInput input,
    HookBatchStartServices services);
  event_loop::Deferred<HookBatchStartStep> next();
private:
  struct State;
  std::shared_ptr<State> state_;
};
} // namespace batbox::instructions::hooks::runtime
