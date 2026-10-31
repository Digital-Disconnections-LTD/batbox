#pragma once
#include "terminal_events/Event.hpp"
#include <any>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace batbox::terminal_events {
struct EventSymbol {};
class EventName {
public:
  EventName(std::string name):value_(std::move(name)){}
  EventName(const char* name):value_(std::string(name)){}
  EventName(std::shared_ptr<EventSymbol> symbol):value_(std::move(symbol)){}
  bool operator==(const EventName& other)const{return value_==other.value_;}
  bool isError()const{
    return std::holds_alternative<std::string>(value_)&&
      std::get<std::string>(value_)=="error";
  }
private:
  std::variant<std::string,std::shared_ptr<EventSymbol>> value_;
};
struct EventArg {
  std::any value;
  std::shared_ptr<Event> event;
  template<class T>static EventArg ordinary(T item){
    return EventArg{std::any(std::move(item)),{}};
  }
  static EventArg fromEvent(std::shared_ptr<Event> item){
    return EventArg{std::any(item),std::move(item)};
  }
};
class EventEmitter {
public:
  using Listener=std::function<void(EventEmitter&,const std::vector<EventArg>&)>;
  using ListenerRef=std::shared_ptr<Listener>;
  EventEmitter();
  void on(const EventName& type,ListenerRef listener);
  void once(const EventName& type,ListenerRef listener);
  void removeListener(const EventName& type,const ListenerRef& listener);
  void off(const EventName& type,const ListenerRef& listener){
    removeListener(type,listener);}
  void removeAllListeners(const EventName& type);
  std::size_t rawListenerCount(const EventName& type)const;
  bool emit(const EventName& type,const std::vector<EventArg>& args={});
  int maxListeners()const{return maxListeners_;}
private:
  struct Registration {
    EventName type;
    ListenerRef listener;
    bool once=false;
    bool active=true;
  };
  using RegistrationRef=std::shared_ptr<Registration>;
  std::vector<RegistrationRef> listeners_;
  int maxListeners_=0;
  std::vector<RegistrationRef> rawListeners(const EventName& type)const;
  void removeExact(const RegistrationRef& registration);
  bool emitNode(const EventName& type,const std::vector<EventArg>& args);
};
} // namespace batbox::terminal_events
