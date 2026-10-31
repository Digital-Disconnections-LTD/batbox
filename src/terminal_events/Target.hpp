#pragma once
#include "terminal_events/Event.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace batbox::terminal_events {
using EventHandler=std::function<void(TerminalEvent&)>;
class HandlerValue {
public:
  HandlerValue()=default;
  HandlerValue(EventHandler callback):callback_(std::move(callback)){}
  template<class F>requires(std::is_invocable_r_v<void,F&,TerminalEvent&>&&
      !std::is_same_v<std::decay_t<F>,HandlerValue>)
  HandlerValue(F callback):callback_(std::move(callback)){}
  HandlerValue(Json value):raw_(std::move(value)){}
  explicit operator bool()const{
    return bool(callback_)||(raw_.has_value()&&value::truthy(*raw_));
  }
  void invoke(TerminalEvent& event)const{
    if(callback_){callback_(event);return;}
    throw std::runtime_error("handler is not a function");
  }
private:
  EventHandler callback_;
  std::optional<Json> raw_;
};
struct EventTarget {
  EventTarget* parentNode=nullptr;
  std::map<std::string,HandlerValue> eventHandlers;
};
} // namespace batbox::terminal_events
