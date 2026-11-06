#pragma once
#include "event_loop/Deferred.hpp"
#include "query/Setup.hpp"

namespace batbox::query {
// A setup stage that changed nothing may resolve this in place of a copy of
// the conversation it was given (the budget result, or "messages" in the
// microcompact and collapse results); the iteration keeps its own copy.
inline Json unchangedMessages(){return Json(Json::value_t::discarded);}
inline bool isUnchangedMessages(const Json& value){return value.is_discarded();}
struct AsyncSetupServices {
  SetupServices sync;
  std::function<void(const std::string&)> queryCheckpoint;
  std::function<void(const std::string&)> headlessProfilerCheckpoint;
  std::function<event_loop::Deferred<Json>(const Json& messages,
    const std::shared_ptr<gadget_results::ContentReplacementState>& state,
    const std::function<void(const Json&)>& persist,
    const std::vector<std::string>& unlimitedGadgets)> applyGadgetResultBudget;
  std::function<event_loop::Deferred<Json>(const Json& messages,
    const execution::Context&,const std::string& querySource)> microcompact;
  std::function<event_loop::Deferred<Json>(const Json& messages,
    const execution::Context&,const Json& stashSafeParams,
    const std::string& querySource,const Json& tracking)> autocompact;
  std::function<event_loop::Deferred<Json>(const Json& messages,
    const execution::Context&,const std::shared_ptr<bats::StashSafeParams>&,
    const std::string& querySource,const Json& tracking)> autocompactIndigenous;
  // Optional: false when autocompact would only report that it skipped, so
  // setup can skip building the compaction snapshot it would ignore.
  std::function<bool(const Json& messages,const execution::Context&,
    const std::string& querySource,const Json& tracking)> autocompactWouldRun;
};

struct AsyncPreparedResult {
  PreparedIteration iteration;
  std::optional<double> missionBudgetRemaining;
};
struct AsyncSetupStep {
  bool done=false;
  Json event;
  std::shared_ptr<AsyncPreparedResult> value;
};
namespace async_setup { struct Run; }

// Demand-driven asynchronous iterator with owner-loop awaits. A request-start
// or compact message pauses all later statements until next().
class AsyncSetupIterator {
public:
  explicit AsyncSetupIterator(std::shared_ptr<async_setup::Run> run):run_(std::move(run)){}
  event_loop::Deferred<AsyncSetupStep> next();
private:
  std::shared_ptr<async_setup::Run> run_;
};
AsyncSetupIterator makeAsyncSetupIterator(event_loop::Loop& loop,Params params,
  State state,AsyncSetupServices services,
  std::optional<double> missionBudgetRemaining={});
} // namespace batbox::query
