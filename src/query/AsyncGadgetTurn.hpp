#pragma once
#include "query/AsyncEngine.hpp"

namespace batbox::query {
struct AsyncGadgetUpdate {
  model::MessageRef message;
  std::optional<execution::Context> newContext;
};
struct AsyncGadgetUpdateCursor {
  std::function<event_loop::Deferred<std::optional<AsyncGadgetUpdate>>()> next;
  std::function<event_loop::Deferred<event_loop::Unit>()> close;
};
struct AsyncMessageCursor {
  std::function<event_loop::Deferred<std::optional<model::MessageRef>>()> next;
  std::function<event_loop::Deferred<event_loop::Unit>()> close;
};
struct AsyncGadgetTurnServices {
  GadgetTurnServices sync;
  std::function<void(const std::string&)> queryCheckpoint;
  std::function<AsyncGadgetUpdateCursor(const std::vector<Json>& gadgetUseBlocks,
    const Json& assistantMessages,const std::shared_ptr<PreparedIteration>&)> gadgetUpdates;
  std::function<AsyncMessageCursor(const Json& queuedCommands,
    const execution::Context&,const Json& history,
    const std::string& querySource)> getAttachmentMessages;
  std::function<event_loop::Deferred<Json>(MemoryPrefetch&)> readMemoryPrefetch;
};
AsyncDecisionCursor makeAsyncGadgetTurnCursor(event_loop::Loop& loop,
  const Params& params,const State& state,
  std::shared_ptr<PreparedIteration> prepared,
  std::shared_ptr<ModelPassResult> modelResult,MemoryPrefetch* memoryPrefetch,
  std::vector<std::string>& consumedCommandUuids,
  AsyncGadgetTurnServices services);
void bindAsyncGadgetTurn(event_loop::Loop& loop,AsyncEngineServices& engine,
  AsyncGadgetTurnServices services);
} // namespace batbox::query
