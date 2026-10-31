#include "terminal_reconciler/HostStatic.hpp"
#include <stdexcept>

namespace batbox::terminal_reconciler {
terminal_events::Dispatcher& HostStatic::dispatcher()const{
  if(!imports_.dispatcher)
    throw std::invalid_argument("Reconciler dispatcher import required");
  return *imports_.dispatcher;
}
int HostStatic::getCurrentUpdatePriority()const{
  return dispatcher().currentUpdatePriority;
}
void HostStatic::setCurrentUpdatePriority(int priority)const{
  dispatcher().currentUpdatePriority=priority;
}
int HostStatic::resolveUpdatePriority()const{
  return dispatcher().resolveEventPriority();
}
std::optional<std::string> HostStatic::resolveEventType()const{
  const auto event=dispatcher().currentEvent;
  return event?std::optional<std::string>(event->type()):
    std::optional<std::string>{};
}
double HostStatic::resolveEventTimeStamp()const{
  const auto event=dispatcher().currentEvent;
  return event?event->timeStamp():-1.1;
}
TimeoutRef HostStatic::scheduleTimeout(
    std::function<void()> callback,double delay)const{
  if(!imports_.scheduleTimeout)
    throw std::invalid_argument("Reconciler timeout scheduler import required");
  return imports_.scheduleTimeout(std::move(callback),delay);
}
void HostStatic::cancelTimeout(const TimeoutRef& timer)const{
  if(!imports_.cancelTimeout)
    throw std::invalid_argument("Reconciler timeout cancellation import required");
  imports_.cancelTimeout(timer);
}
} // namespace batbox::terminal_reconciler
