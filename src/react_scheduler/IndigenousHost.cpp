#include "react_scheduler/Scheduler.hpp"
#include <chrono>

namespace batbox::react_scheduler {
Host indigenousHost(event_loop::Loop& loop,
    std::function<void(const std::string&)> consoleError){
  using Clock=std::chrono::steady_clock;
  static const auto origin=Clock::now();
  return Host{
    []{return std::chrono::duration<double,std::milli>(
      Clock::now()-origin).count();},
    [&loop](std::function<void()> mission){loop.queueNextTurn(std::move(mission));},
    [&loop](std::function<void()> mission,double delay)->TimerRef{
      return std::make_shared<event_loop::Loop::TimerHandle>(
        loop.setTimeout(delay,std::move(mission)));
    },
    [](const TimerRef& timer){
      if(timer)std::static_pointer_cast<event_loop::Loop::TimerHandle>(timer)
        ->cancel();
    },
    std::move(consoleError)
  };
}
} // namespace batbox::react_scheduler
