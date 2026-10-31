#pragma once
#include "event_loop/Deferred.hpp"
#include "query/ModelPass.hpp"
#include <atomic>

namespace batbox::query {
struct AsyncModelCursor {
  std::function<event_loop::Deferred<std::optional<model::MessageRef>>()> next;
  std::function<event_loop::Deferred<event_loop::Unit>()> close;
};
struct AsyncModelServices {
  ModelServices sync;
  std::function<AsyncModelCursor(const ModelRequest&,
    const std::function<void()>& onStreamingFallback)> callModel;
  std::function<void(const std::string&)> queryCheckpoint;
  std::function<void(const std::exception&)> logError,logInternalError;
};
struct AsyncModelStep {
  bool done=false;
  model::MessageRef event;
  std::shared_ptr<ModelPassResult> value;
  // The tombstone holds a message object, not just its serialized value.
  model::MessageRef tombstoneTarget={};
};
namespace async_model { struct Run; }
class AsyncModelPassIterator {
public:
  explicit AsyncModelPassIterator(std::shared_ptr<async_model::Run> run):run_(std::move(run)){}
  event_loop::Deferred<AsyncModelStep> next();
  event_loop::Deferred<event_loop::Unit> close();
  event_loop::Deferred<AsyncModelStep> throwEarly(std::exception_ptr error);
private:
  std::shared_ptr<async_model::Run> run_;
};
AsyncModelPassIterator makeAsyncModelPassIterator(event_loop::Loop& loop,
  Params params,State state,std::shared_ptr<PreparedIteration> prepared,
  QueryConfig config,std::optional<double> missionBudgetRemaining,
  AsyncModelServices services);
} // namespace batbox::query
