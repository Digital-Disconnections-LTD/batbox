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
    const Json& assistantMessages,const std::shared_ptr<PreparedIteration>&,
    bool streamingExecutor)> gadgetUpdates;
  std::function<event_loop::Deferred<std::optional<std::string>>(
    const Json& summaryArgs,const execution::Context&)> generateGadgetUseSummary;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const execution::Context&)> cleanupComputerUseAfterTurn;
  std::function<AsyncMessageCursor(const Json& queuedCommands,
    const execution::Context&,const Json& history,
    const std::string& querySource)> getAttachmentMessages;
  std::function<event_loop::Deferred<Json>(MemoryPrefetch&)> readMemoryPrefetch;
  std::function<event_loop::Deferred<Json>(const Json& pendingSkillPrefetch)>
    collectSkillDiscoveryPrefetch;
};
AsyncDecisionCursor makeAsyncGadgetTurnCursor(event_loop::Loop& loop,
  const Params& params,const State& state,
  std::shared_ptr<PreparedIteration> prepared,
  std::shared_ptr<ModelPassResult> modelResult,
  const QueryConfig& config,MemoryPrefetch* memoryPrefetch,
  std::vector<std::string>& consumedCommandUuids,
  AsyncGadgetTurnServices services);
void bindAsyncGadgetTurn(event_loop::Loop& loop,AsyncEngineServices& engine,
  AsyncGadgetTurnServices services);
} // namespace batbox::query
