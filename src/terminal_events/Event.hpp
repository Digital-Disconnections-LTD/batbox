#pragma once
#include <functional>
#include <optional>
#include <string>

namespace batbox::terminal_events {
class Event {
public:
  virtual ~Event()=default;
  bool didStopImmediatePropagation()const{return stoppedImmediate_;}
  virtual void stopImmediatePropagation(){stoppedImmediate_=true;}
private:
  bool stoppedImmediate_=false;
};

struct EventTarget;
struct TerminalEventInit {
  std::optional<bool> bubbles,cancelable;
};
using MonotonicNow=std::function<double()>;
class TerminalEvent:public Event {
public:
  TerminalEvent(std::string type,MonotonicNow now,
    TerminalEventInit init={});
  const std::string& type()const{return type_;}
  double timeStamp()const{return timeStamp_;}
  bool bubbles()const{return bubbles_;}
  bool cancelable()const{return cancelable_;}
  EventTarget* target()const{return target_;}
  EventTarget* currentTarget()const{return currentTarget_;}
  const std::string& eventPhase()const{return phase_;}
  bool defaultPrevented()const{return defaultPrevented_;}
  void stopPropagation(){propagationStopped_=true;}
  void stopImmediatePropagation()override;
  void preventDefault();
  void setTarget(EventTarget* target){target_=target;}
  void setCurrentTarget(EventTarget* target){currentTarget_=target;}
  void setEventPhase(std::string phase){phase_=std::move(phase);}
  bool isPropagationStopped()const{return propagationStopped_;}
  bool isImmediatePropagationStopped()const{
    return didStopImmediatePropagation();}
  virtual void prepareForTarget(EventTarget*){}
private:
  std::string type_;
  double timeStamp_;
  bool bubbles_,cancelable_;
  EventTarget* target_=nullptr;
  EventTarget* currentTarget_=nullptr;
  std::string phase_="none";
  bool propagationStopped_=false,defaultPrevented_=false;
};

class FocusEvent:public TerminalEvent {
public:
  FocusEvent(std::string type,MonotonicNow now,
    EventTarget* relatedTarget=nullptr);
  EventTarget* relatedTarget()const{return relatedTarget_;}
private:
  EventTarget* relatedTarget_;
};
class ClickEvent:public Event {
public:
  ClickEvent(double col,double row,bool cellIsBlank):
    col(col),row(row),cellIsBlank(cellIsBlank){}
  const double col,row;
  double localCol=0,localRow=0;
  const bool cellIsBlank;
};
class TerminalFocusEvent:public Event {
public:
  explicit TerminalFocusEvent(std::string type):type(std::move(type)){}
  const std::string type;
};
} // namespace batbox::terminal_events
