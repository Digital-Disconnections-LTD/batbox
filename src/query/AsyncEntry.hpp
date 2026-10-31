#pragma once
#include "query/AsyncEngine.hpp"
#include "query/Entry.hpp"

namespace batbox::query {
struct AsyncQueryCursor {
  std::function<event_loop::Deferred<AsyncEngineStep>()> next;
  std::function<event_loop::Deferred<AsyncEngineStep>(const Json&)> returnEarly;
  std::function<event_loop::Deferred<AsyncEngineStep>(std::exception_ptr)> throwEarly;
};
struct AsyncQueryImports {
  std::function<AsyncQueryCursor(Params,std::vector<std::string>&)> queryLoop;
  NotifyLifecycle notifyCommandLifecycle;
};

// The public query generator owns the consumed-command list for its inner
// loop, including while next()/return()/throw() are queued across an await.
class AsyncQueryIterator {
public:
  explicit AsyncQueryIterator(std::shared_ptr<AsyncOperationQueue> operations);
  AsyncQueryIterator(const AsyncQueryIterator&)=delete;
  AsyncQueryIterator& operator=(const AsyncQueryIterator&)=delete;
  AsyncQueryIterator(AsyncQueryIterator&&)=default;
  AsyncQueryIterator& operator=(AsyncQueryIterator&&)=default;
  event_loop::Deferred<AsyncEngineStep> next();
  event_loop::Deferred<AsyncEngineStep> returnEarly(Json value);
  event_loop::Deferred<AsyncEngineStep> throwEarly(std::exception_ptr error);
private:
  std::shared_ptr<AsyncOperationQueue> operations_;
};

AsyncQueryIterator makeAsyncQuery(event_loop::Loop& loop,Params params,
  AsyncQueryImports imports);
AsyncQueryIterator makeAsyncQuery(event_loop::Loop& loop,Params params,
  AsyncEngineServices services,NotifyLifecycle notifyCommandLifecycle);
} // namespace batbox::query
