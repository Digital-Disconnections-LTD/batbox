#pragma once
#include "event_loop/Deferred.hpp"
#include "query/Setup.hpp"

namespace batbox::query {
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
    const execution::Context&,const std::string& querySource)> applyCollapsesIfNeeded;
  std::function<event_loop::Deferred<Json>(const Json& messages,
    const execution::Context&,const Json& stashSafeParams,
    const std::string& querySource,const Json& tracking,double snipTokensFreed)> autocompact;
  std::function<event_loop::Deferred<Json>(const Json& messages,
    const execution::Context&,const std::shared_ptr<bats::StashSafeParams>&,
    const std::string& querySource,const Json& tracking,double snipTokensFreed)> autocompactIndigenous;
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

// Demand-driven asynchronous iterator with owner-loop awaits. A request-start,
// snip boundary, or compact message pauses all later statements until next().
class AsyncSetupIterator {
public:
  explicit AsyncSetupIterator(std::shared_ptr<async_setup::Run> run):run_(std::move(run)){}
  event_loop::Deferred<AsyncSetupStep> next();
private:
  std::shared_ptr<async_setup::Run> run_;
};
AsyncSetupIterator makeAsyncSetupIterator(event_loop::Loop& loop,Params params,
  State state,QueryConfig config,AsyncSetupServices services,
  std::optional<double> missionBudgetRemaining={});
} // namespace batbox::query
