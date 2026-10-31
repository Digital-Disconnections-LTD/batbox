#pragma once
#include "query/AsyncEngine.hpp"
#include <deque>

namespace batbox::query {
// Indigenous language adapter for async-generator method call order. Operations
// are serialized even when the caller invokes next()/return()/throw() before
// an earlier next() settles. The callbacks retain the generator's
// stage-specific behavior, including throw handlers and nested cleanup.
class AsyncOperationQueue final:
    public std::enable_shared_from_this<AsyncOperationQueue> {
public:
  using Next=std::function<event_loop::Deferred<AsyncEngineStep>()>;
  using Return=std::function<event_loop::Deferred<AsyncEngineStep>(const Json&)>;
  using Throw=std::function<event_loop::Deferred<AsyncEngineStep>(std::exception_ptr)>;
  AsyncOperationQueue(event_loop::Loop& loop,Next next,Return returnEarly,
    Throw throwEarly,bool drainImmediately=false);
  event_loop::Deferred<AsyncEngineStep> next();
  event_loop::Deferred<AsyncEngineStep> returnEarly(Json value);
  event_loop::Deferred<AsyncEngineStep> throwEarly(std::exception_ptr error);
private:
  enum class Kind {Next,Return,Throw};
  struct Operation {
    Kind kind;
    Json value;
    std::exception_ptr error;
    event_loop::Deferred<AsyncEngineStep> result;
  };
  event_loop::Loop& loop_;
  Next next_;
  Return return_;
  Throw throw_;
  std::deque<Operation> pending_;
  bool active_=false;
  bool closed_=false;
  // A yield* delegate can enter the next method before the prior consumer
  // reaction. Lowered inner loops retain their explicit yield microtask.
  bool drainImmediately_=false;
  void enqueue(Operation operation);
  void pump();
  void settle(const AsyncEngineStep* value,const std::exception_ptr& error);
  void advance();
};
} // namespace batbox::query
