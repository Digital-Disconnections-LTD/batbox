#include "terminal_events/Emitter.hpp"
#include <stdexcept>

namespace batbox::terminal_events {
EventEmitter::EventEmitter()=default;
void EventEmitter::on(const EventName& type,ListenerRef listener){
  if(!listener||!*listener)
    throw std::invalid_argument("Event listener required");
  listeners_.push_back(std::make_shared<Registration>(
    Registration{type,std::move(listener),false,true}));
}
void EventEmitter::once(const EventName& type,ListenerRef listener){
  if(!listener||!*listener)
    throw std::invalid_argument("Event listener required");
  listeners_.push_back(std::make_shared<Registration>(
    Registration{type,std::move(listener),true,true}));
}
void EventEmitter::removeExact(const RegistrationRef& registration){
  if(!registration||!registration->active)return;
  registration->active=false;
  for(auto it=listeners_.begin();it!=listeners_.end();++it)
    if(*it==registration){listeners_.erase(it);return;}
}
void EventEmitter::removeListener(const EventName& type,
    const ListenerRef& listener){
  for(auto it=listeners_.rbegin();it!=listeners_.rend();++it)
    if((*it)->active&&(*it)->type==type&&(*it)->listener==listener){
      removeExact(*it);return;
    }
}
void EventEmitter::removeAllListeners(const EventName& type){
  for(auto it=listeners_.begin();it!=listeners_.end();){
    if((*it)->type==type){(*it)->active=false;it=listeners_.erase(it);}
    else ++it;
  }
}
std::vector<EventEmitter::RegistrationRef> EventEmitter::rawListeners(
    const EventName& type)const{
  std::vector<RegistrationRef> result;
  for(const auto& listener:listeners_)
    if(listener->active&&listener->type==type)result.push_back(listener);
  return result;
}
std::size_t EventEmitter::rawListenerCount(const EventName& type)const{
  return rawListeners(type).size();
}
bool EventEmitter::emitNode(const EventName& type,
    const std::vector<EventArg>& args){
  const auto listeners=rawListeners(type);
  if(listeners.empty()){
    if(type.isError()){
      if(!args.empty()&&args[0].value.type()==typeid(std::exception_ptr))
        std::rethrow_exception(std::any_cast<std::exception_ptr>(args[0].value));
      throw std::runtime_error("Unhandled error.");
    }
    return false;
  }
  for(const auto& listener:listeners){
    if(listener->once)removeExact(listener);
    (*listener->listener)(*this,args);
  }
  return true;
}
bool EventEmitter::emit(const EventName& type,
    const std::vector<EventArg>& args){
  if(type.isError())return emitNode(type,args);
  const auto listeners=rawListeners(type);
  if(listeners.empty())return false;
  const auto ccEvent=!args.empty()?args[0].event:std::shared_ptr<Event>{};
  for(const auto& listener:listeners){
    if(listener->once)removeExact(listener);
    (*listener->listener)(*this,args);
    if(ccEvent&&ccEvent->didStopImmediatePropagation())break;
  }
  return true;
}
} // namespace batbox::terminal_events
