#include "terminal_events/Event.hpp"
#include <stdexcept>

namespace batbox::terminal_events {
TerminalEvent::TerminalEvent(std::string type,MonotonicNow now,
    TerminalEventInit init):type_(std::move(type)),timeStamp_([&]{
      if(!now)throw std::invalid_argument("Terminal event clock required");
      return now();}()),
    bubbles_(init.bubbles.value_or(true)),
    cancelable_(init.cancelable.value_or(true)){}
void TerminalEvent::stopImmediatePropagation(){
  Event::stopImmediatePropagation();
  propagationStopped_=true;
}
void TerminalEvent::preventDefault(){
  if(cancelable_)defaultPrevented_=true;
}
FocusEvent::FocusEvent(std::string type,MonotonicNow now,
    EventTarget* relatedTarget):
  TerminalEvent(std::move(type),std::move(now),
    TerminalEventInit{true,false}),relatedTarget_(relatedTarget){}
} // namespace batbox::terminal_events
