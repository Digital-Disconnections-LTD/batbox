#include "query/OwnerWatchdogTimer.hpp"
#include "query/OwnerSegmentInvoker.hpp"

namespace batbox::query {
model::StreamWatchdog::TimerScheduler makeOwnerWatchdogTimer(
    event_loop::Loop& loop){
  const auto dispatcher=loop.dispatcher();
  return [dispatcher,&loop](double delay,std::function<void()> callback){
    return invokeOwnerSegment(dispatcher,{},[&]{
      const auto timer=loop.setTimeout(delay,std::move(callback));
      return std::function<void()>([dispatcher,timer]{
        (void)invokeOwnerSegment(dispatcher,{},[&]{
          timer.cancel();return event_loop::Unit{};
        });
      });
    });
  };
}
}
