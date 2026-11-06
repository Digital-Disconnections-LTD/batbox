#pragma once
#include "query/AsyncEngine.hpp"

namespace batbox::query {
struct AsyncStopStep {
  bool done=false;
  model::MessageRef event;
  Json result;
};
struct AsyncStopCursor {
  std::function<event_loop::Deferred<AsyncStopStep>()> next;
  std::function<event_loop::Deferred<event_loop::Unit>()> close;
};
struct AsyncCompletionServices {
  CompletionServices sync;
  std::function<AsyncStopCursor(const Json& messagesForQuery,
    const Json& assistantMessages,const Params&,const execution::Context&,
    std::optional<bool> stopHookActive)> handleStopHooks;
};
// Demand-driven cursor for the query loop's no-Gadget branch.
AsyncDecisionCursor makeAsyncCompletionCursor(event_loop::Loop& loop,
  const Params& params,const State& state,
  std::shared_ptr<PreparedIteration> prepared,
  std::shared_ptr<ModelPassResult> modelResult,
  AsyncCompletionServices services);
void bindAsyncCompletion(event_loop::Loop& loop,AsyncEngineServices& engine,
  AsyncCompletionServices services);
} // namespace batbox::query
