#include "terminal_dom/Focus.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::terminal_dom {
namespace {
constexpr std::size_t MAX_FOCUS_STACK=32;
}
FocusManager::FocusManager(FocusDispatch dispatchFocusEvent,
    terminal_events::MonotonicNow now):
  dispatchFocusEvent_(std::move(dispatchFocusEvent)),now_(std::move(now)){
  if(!dispatchFocusEvent_)throw std::invalid_argument("Focus event dispatcher required");
  if(!now_)throw std::invalid_argument("Focus event clock required");
}
void FocusManager::focus(const ElementRef& node){
  if(!node)throw std::invalid_argument("Focus target required");
  if(node.get()==activeElement.get())return;
  if(!enabled_)return;
  const auto previous=activeElement;
  if(previous){
    const auto found=std::find_if(focusStack_.begin(),focusStack_.end(),
      [&](const ElementRef& value){return value.get()==previous.get();});
    if(found!=focusStack_.end())focusStack_.erase(found);
    focusStack_.push_back(previous);
    if(focusStack_.size()>MAX_FOCUS_STACK)focusStack_.erase(focusStack_.begin());
    terminal_events::FocusEvent event("blur",now_,node.get());
    dispatchFocusEvent_(*previous,event);
  }
  activeElement=node;
  terminal_events::FocusEvent event("focus",now_,previous.get());
  dispatchFocusEvent_(*node,event);
}
void FocusManager::blur(){
  if(!activeElement)return;
  const auto previous=activeElement;
  activeElement.reset();
  terminal_events::FocusEvent event("blur",now_,nullptr);
  dispatchFocusEvent_(*previous,event);
}
void FocusManager::handleNodeRemoved(const ElementRef& node,DOMElement& root){
  if(!node)throw std::invalid_argument("Removed focus node required");
  std::vector<ElementRef> retained;retained.reserve(focusStack_.size());
  for(const auto& value:focusStack_)
    if(value.get()!=node.get()&&isInTree(*value,root))retained.push_back(value);
  focusStack_=std::move(retained);
  if(!activeElement)return;
  if(activeElement.get()!=node.get()&&isInTree(*activeElement,root))return;
  const auto removed=activeElement;
  activeElement.reset();
  terminal_events::FocusEvent blurEvent("blur",now_,nullptr);
  dispatchFocusEvent_(*removed,blurEvent);
  while(!focusStack_.empty()){
    const auto candidate=focusStack_.back();focusStack_.pop_back();
    if(isInTree(*candidate,root)){
      activeElement=candidate;
      terminal_events::FocusEvent focusEvent("focus",now_,removed.get());
      dispatchFocusEvent_(*candidate,focusEvent);
      return;
    }
  }
}
void FocusManager::handleAutoFocus(const ElementRef& node){focus(node);}
void FocusManager::handleClickFocus(const ElementRef& node){
  if(!node)throw std::invalid_argument("Click focus node required");
  if(!node->attributes.is_object())return;
  const auto found=node->attributes.find("tabIndex");
  if(found==node->attributes.end()||!found->is_number())return;
  focus(node);
}
void FocusManager::enable(){enabled_=true;}
void FocusManager::disable(){enabled_=false;}
void FocusManager::focusNext(DOMElement& root){moveFocus(1,root);}
void FocusManager::focusPrevious(DOMElement& root){moveFocus(-1,root);}
void FocusManager::moveFocus(int direction,DOMElement& root){
  if(!enabled_)return;
  const auto tabbable=collectTabbable(root);
  if(tabbable.empty())return;
  std::size_t currentIndex=tabbable.size();
  if(activeElement){
    for(std::size_t i=0;i<tabbable.size();++i)
      if(tabbable[i].get()==activeElement.get()){
        currentIndex=i;break;
      }
  }
  const auto nextIndex=currentIndex==tabbable.size()?
    direction==1?std::size_t{0}:tabbable.size()-1:
    (currentIndex+tabbable.size()+direction)%tabbable.size();
  const auto next=tabbable[nextIndex];
  if(next)focus(next);
}
}
