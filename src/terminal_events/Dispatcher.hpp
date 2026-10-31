#pragma once
#include "terminal_events/Target.hpp"
#include <exception>
#include <vector>

namespace batbox::terminal_events {
struct HandlerProps {
  std::string bubble,capture;
};
const HandlerProps* handlerForEvent(const std::string& type);
bool isEventHandlerProp(const std::string& property);

struct DispatchListener {
  EventTarget* node=nullptr;
  HandlerValue handler;
  std::string phase;
};
HandlerValue getHandler(EventTarget& node,const std::string& eventType,
  bool capture);
std::vector<DispatchListener> collectListeners(EventTarget& target,
  const TerminalEvent& event);
void processDispatchQueue(const std::vector<DispatchListener>& listeners,
  TerminalEvent& event,
  const std::function<void(std::exception_ptr)>& logError);

struct Priorities {
  int discrete,continuous,normal,none;
};
int getEventPriority(const std::string& eventType,const Priorities& priorities);
using DispatchThunk=std::function<bool(EventTarget&,TerminalEvent&)>;
struct UndefinedArg {};
using DiscreteUpdates=std::function<bool(const DispatchThunk&,
  EventTarget&,TerminalEvent&,UndefinedArg,UndefinedArg)>;
class Dispatcher {
public:
  Dispatcher(Priorities priorities,
    std::function<void(std::exception_ptr)> logError);
  TerminalEvent* currentEvent=nullptr;
  int currentUpdatePriority;
  DiscreteUpdates discreteUpdates;
  int resolveEventPriority()const;
  bool dispatch(EventTarget& target,TerminalEvent& event);
  bool dispatchDiscrete(EventTarget& target,TerminalEvent& event);
  bool dispatchContinuous(EventTarget& target,TerminalEvent& event);
private:
  Priorities priorities_;
  std::function<void(std::exception_ptr)> logError_;
};
} // namespace batbox::terminal_events
