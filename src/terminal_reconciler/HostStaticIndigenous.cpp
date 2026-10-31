#include "terminal_reconciler/HostStatic.hpp"
#include "event_loop/Loop.hpp"

namespace batbox::terminal_reconciler {
HostStaticImports indigenousHostStaticImports(
    terminal_events::Dispatcher& dispatcher,event_loop::Loop& loop){
  HostStaticImports imports;
  imports.dispatcher=&dispatcher;
  imports.scheduleTimeout=[&loop](std::function<void()> callback,
      double delay)->TimeoutRef{
    return std::make_shared<event_loop::Loop::TimerHandle>(
      loop.setTimeout(delay,std::move(callback)));
  };
  imports.cancelTimeout=[](const TimeoutRef& timer){
    if(timer)
      std::static_pointer_cast<event_loop::Loop::TimerHandle>(timer)->cancel();
  };
  return imports;
}
} // namespace batbox::terminal_reconciler
