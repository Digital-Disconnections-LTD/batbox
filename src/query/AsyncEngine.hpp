#pragma once
#include "query/AsyncModelPass.hpp"
#include "query/AsyncSetup.hpp"
#include "query/Engine.hpp"

namespace batbox::query {
struct AsyncDecisionStep {
  bool done=false;
  model::MessageRef event;
  std::shared_ptr<CompletionDecision> value;
};
struct AsyncDecisionCursor {
  std::function<event_loop::Deferred<AsyncDecisionStep>()> next;
  std::function<event_loop::Deferred<event_loop::Unit>()> close;
};
struct AsyncEngineServices {
  ConfigDependencies config;
  AsyncSetupServices setup;
  AsyncModelServices model;
  CompletionServices completion;
  GadgetTurnServices gadgetTurn;
  std::function<MemoryPrefetch(const Json&,const execution::Context&)>
    startRelevantMemoryPrefetch;
  // Launch post-sampling hooks without awaiting them.
  std::function<void(const Json&,const Params&,const execution::Context&)>
    executePostSamplingHooks;
  std::function<AsyncModelCursor(const std::shared_ptr<PreparedIteration>&)>
    abortRemainingResults;
  std::function<event_loop::Deferred<event_loop::Unit>(const execution::Context&)>
    cleanupComputerUseAfterTurn;
  std::function<event_loop::Deferred<Json>(const State&)> awaitPendingGadgetUseSummary;
  // Bind the indigenous demand-driven regions with bindAsyncCompletion and
  // bindAsyncGadgetTurn; synchronous callback wrappers are not required here.
  std::function<AsyncDecisionCursor(const Params&,const State&,
    std::shared_ptr<PreparedIteration>,std::shared_ptr<ModelPassResult>,
    std::optional<double>&,BudgetTracker*)> completeWithoutGadgets;
  std::function<AsyncDecisionCursor(const Params&,const State&,
    std::shared_ptr<PreparedIteration>,std::shared_ptr<ModelPassResult>,
    const QueryConfig&,MemoryPrefetch*,
    std::vector<std::string>&)> executeGadgetTurn;
  // Indigenous recovery extension: refresh the existing context after a successful
  // compact, before the next request, without appending another copy to history.
  std::function<event_loop::Deferred<Json>(const Json&,const execution::Context&)>
    refreshUserContextAfterCompact;
};
struct AsyncEngineStep {
  bool done=false;
  model::MessageRef event;
  std::optional<Json> terminal;
  // Presentation identity is carried outside the serializable event payload.
  model::MessageRef tombstoneTarget={};
};
namespace async_engine { struct Run; }
class AsyncOperationQueue;
class AsyncEngineIterator {
public:
  explicit AsyncEngineIterator(std::shared_ptr<async_engine::Run> run):run_(std::move(run)){}
  AsyncEngineIterator(const AsyncEngineIterator&)=delete;
  AsyncEngineIterator& operator=(const AsyncEngineIterator&)=delete;
  AsyncEngineIterator(AsyncEngineIterator&&)=default;
  AsyncEngineIterator& operator=(AsyncEngineIterator&&)=default;
  event_loop::Deferred<AsyncEngineStep> next();
  event_loop::Deferred<AsyncEngineStep> returnEarly(Json value);
  event_loop::Deferred<AsyncEngineStep> throwEarly(std::exception_ptr error);
private:
  std::shared_ptr<async_engine::Run> run_;
  std::shared_ptr<AsyncOperationQueue> operations_;
  void ensureOperations();
};
AsyncEngineIterator makeAsyncQueryLoop(event_loop::Loop& loop,Params params,
  std::vector<std::string>& consumedCommandUuids,AsyncEngineServices services);
} // namespace batbox::query
