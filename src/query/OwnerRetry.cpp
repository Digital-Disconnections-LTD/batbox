#include "query/OwnerRetry.hpp"
#include "query/OwnerSegmentInvoker.hpp"
#include "retry/Machine.hpp"
#include "retry/Internal.hpp"

namespace {
struct OwnerSleep {
  batbox::event_loop::Deferred<batbox::event_loop::Unit> promise;
  batbox::event_loop::Loop::TimerHandle timer;
};
OwnerSleep startOwnerSleep(batbox::event_loop::Loop& loop,double delayMs,
    std::stop_token signal) {
  OwnerSleep pending{batbox::event_loop::Deferred<batbox::event_loop::Unit>(loop),{}};
  if(signal.stop_requested()) {
    pending.promise.reject(std::make_exception_ptr(batbox::retry::internal::abortError()));
    return pending;
  }
  pending.timer=loop.setTimeout(delayMs,[promise=pending.promise]{
    promise.resolve(batbox::event_loop::Unit{});
  },true);
  return pending;
}
}

namespace batbox::query {

Json withRetryOwnerSegments(event_loop::Loop& loop,
    const retry::RetryOptions& options,retry::Dependencies& deps,
    const OwnerRetryAwaits& awaits,std::stop_token cursorClose) {
  const auto owner=loop.dispatcher();
  if(owner.isOwnerThread())
    throw std::logic_error("Owner retry driver cannot block the owner loop");
  const auto stop=cursorClose;
  auto machine=invokeOwnerSegment(owner,stop,[&]{
    return std::make_shared<retry::RetryMachine>(options,deps);
  });
  while(true) {
    const auto step=invokeOwnerSegment(owner,stop,[&]{return machine->advance();});
    switch(step.kind) {
      case retry::RetryStep::Kind::RefreshOAuth:
        try {
          auto pending=invokeOwnerSegment(owner,stop,[&]{
            if(awaits.refreshOAuth)return awaits.refreshOAuth(step.accessToken);
            event_loop::Deferred<event_loop::Unit> ready(loop);
            retry::internal::require(deps.handleOAuth401Error,"handleOAuth401Error");
            deps.handleOAuth401Error(step.accessToken);
            ready.resolve(event_loop::Unit{});
            return ready;
          });
          (void)waitOwnerDeferred(owner,stop,pending);
          (void)invokeOwnerSegment(owner,stop,[&]{
            machine->refreshedOAuth();return event_loop::Unit{};
          });
        } catch (...) {
          if(stop.stop_requested())throw;
          auto failure=std::current_exception();
          (void)invokeOwnerSegment(owner,stop,[&]{
            machine->failed(failure);return event_loop::Unit{};
          });
        }
        break;
      case retry::RetryStep::Kind::GetClient:
        try {
          Json client=deps.getClient();
          (void)invokeOwnerSegment(owner,stop,[&]{
            machine->receivedClient(std::move(client));return event_loop::Unit{};
          });
        } catch (...) {
          if(stop.stop_requested())throw;
          auto failure=std::current_exception();
          (void)invokeOwnerSegment(owner,stop,[&]{
            machine->failed(failure);return event_loop::Unit{};
          });
        }
        break;
      case retry::RetryStep::Kind::Operation:
        try {
          Json result=deps.operation(step.value,step.attempt,step.context);
          (void)invokeOwnerSegment(owner,stop,[&]{
            machine->receivedOperation(std::move(result));return event_loop::Unit{};
          });
        } catch (...) {
          if(stop.stop_requested())throw;
          auto failure=std::current_exception();
          (void)invokeOwnerSegment(owner,stop,[&]{
            machine->failed(failure);return event_loop::Unit{};
          });
        }
        break;
      case retry::RetryStep::Kind::Yield:
        retry::internal::require(deps.yield,"yield");
        deps.yield(step.value);
        (void)invokeOwnerSegment(owner,stop,[&]{
          machine->resumedYield();return event_loop::Unit{};
        });
        break;
      case retry::RetryStep::Kind::Sleep:
        retry::internal::checkAbort(options.signal);
        if(awaits.sleep) {
          auto pending=invokeOwnerSegment(owner,stop,[&]{
            return awaits.sleep(step.delayMs,options.signal);
          });
          (void)waitOwnerDeferred(owner,stop,pending);
        } else if(deps.sleep)deps.sleep(step.delayMs,options.signal);
        else {
          auto pending=invokeOwnerSegment(owner,stop,[&]{
            return startOwnerSleep(loop,step.delayMs,options.signal);
          });
          std::stop_callback onAbort(options.signal,[pending]{
            pending.timer.cancel();
            pending.promise.reject(std::make_exception_ptr(retry::internal::abortError()));
          });
          try {(void)waitOwnerDeferred(owner,stop,pending.promise);}
          catch (...) {pending.timer.cancel();throw;}
          pending.timer.cancel();
        }
        retry::internal::checkAbort(options.signal);
        (void)invokeOwnerSegment(owner,stop,[&]{
          machine->completedSleep();return event_loop::Unit{};
        });
        break;
      case retry::RetryStep::Kind::Complete:
        return step.value;
    }
  }
}

} // namespace batbox::query
