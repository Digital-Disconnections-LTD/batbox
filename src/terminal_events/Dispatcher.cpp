#include "terminal_events/Dispatcher.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::terminal_events {
HandlerValue getHandler(EventTarget& node,const std::string& eventType,
    bool capture){
  const auto* mapping=handlerForEvent(eventType);
  if(!mapping)return {};
  const auto& property=capture?mapping->capture:mapping->bubble;
  if(property.empty())return {};
  const auto found=node.eventHandlers.find(property);
  return found==node.eventHandlers.end()?HandlerValue{}:found->second;
}
std::vector<DispatchListener> collectListeners(EventTarget& target,
    const TerminalEvent& event){
  std::vector<DispatchListener> listeners;
  for(auto* node=&target;node;node=node->parentNode){
    const bool isTarget=node==&target;
    const auto capture=getHandler(*node,event.type(),true);
    const auto bubble=getHandler(*node,event.type(),false);
    if(capture)
      listeners.insert(listeners.begin(),DispatchListener{
        node,capture,isTarget?"at_target":"capturing"});
    if(bubble&&(event.bubbles()||isTarget))
      listeners.push_back(DispatchListener{
        node,bubble,isTarget?"at_target":"bubbling"});
  }
  return listeners;
}
void processDispatchQueue(const std::vector<DispatchListener>& listeners,
    TerminalEvent& event,
    const std::function<void(std::exception_ptr)>& logError){
  EventTarget* previousNode=nullptr;
  for(const auto& listener:listeners){
    if(event.isImmediatePropagationStopped())break;
    if(event.isPropagationStopped()&&listener.node!=previousNode)break;
    event.setEventPhase(listener.phase);
    event.setCurrentTarget(listener.node);
    event.prepareForTarget(listener.node);
    try{listener.handler.invoke(event);}
    catch(...){
      if(!logError)throw std::invalid_argument("Dispatch error logger required");
      logError(std::current_exception());
    }
    previousNode=listener.node;
  }
}
int getEventPriority(const std::string& eventType,
    const Priorities& priorities){
  if(eventType=="keydown"||eventType=="keyup"||eventType=="click"||
      eventType=="focus"||eventType=="blur"||eventType=="paste")
    return priorities.discrete;
  if(eventType=="resize"||eventType=="scroll"||eventType=="mousemove")
    return priorities.continuous;
  return priorities.normal;
}
Dispatcher::Dispatcher(Priorities priorities,
    std::function<void(std::exception_ptr)> logError):
  currentUpdatePriority(priorities.normal),priorities_(priorities),
  logError_(std::move(logError)){}
int Dispatcher::resolveEventPriority()const{
  if(currentUpdatePriority!=priorities_.none)return currentUpdatePriority;
  if(currentEvent)return getEventPriority(currentEvent->type(),priorities_);
  return priorities_.normal;
}
bool Dispatcher::dispatch(EventTarget& target,TerminalEvent& event){
  const auto previousEvent=currentEvent;
  currentEvent=&event;
  try{
    event.setTarget(&target);
    const auto listeners=collectListeners(target,event);
    processDispatchQueue(listeners,event,logError_);
    event.setEventPhase("none");
    event.setCurrentTarget(nullptr);
    const bool result=!event.defaultPrevented();
    currentEvent=previousEvent;
    return result;
  }catch(...){currentEvent=previousEvent;throw;}
}
bool Dispatcher::dispatchDiscrete(EventTarget& target,TerminalEvent& event){
  if(!discreteUpdates)return dispatch(target,event);
  return discreteUpdates([this](EventTarget& node,TerminalEvent& value){
    return dispatch(node,value);},target,event,{},{});
}
bool Dispatcher::dispatchContinuous(EventTarget& target,TerminalEvent& event){
  const auto previousPriority=currentUpdatePriority;
  try{
    currentUpdatePriority=priorities_.continuous;
    const bool result=dispatch(target,event);
    currentUpdatePriority=previousPriority;
    return result;
  }catch(...){currentUpdatePriority=previousPriority;throw;}
}
} // namespace batbox::terminal_events
