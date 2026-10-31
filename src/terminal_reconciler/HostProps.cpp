#include "terminal_reconciler/HostProps.hpp"
#include <stdexcept>

namespace batbox::terminal_reconciler {
void setEventHandler(terminal_dom::DOMElement& node,
    const std::string& key,const HostPropValue& value,
    terminal_hit_test::PointerHandlerStore* pointerHandlers){
  if(value.handler)node.eventHandlers[key]=*value.handler;
  else if(!value.value||value.value->is_discarded())
    node.eventHandlers[key]=terminal_events::HandlerValue{};
  else node.eventHandlers[key]=terminal_events::HandlerValue(*value.value);
  if(key=="onClick"||key=="onMouseEnter"||key=="onMouseLeave"){
    if((value.onClick||value.onHover)&&!pointerHandlers)
      throw std::invalid_argument("Pointer handler store import required");
    if(pointerHandlers){
      const auto ref=std::static_pointer_cast<terminal_dom::DOMElement>(
        node.shared_from_this());
      if(key=="onClick")pointerHandlers->setClick(ref,value.onClick);
      else if(key=="onMouseEnter")
        pointerHandlers->setEnter(ref,value.onHover);
      else pointerHandlers->setLeave(ref,value.onHover);
    }
  }
}
void applyProp(terminal_dom::DOMElement& node,const std::string& key,
    const HostPropValue& value,const HostPropImports& imports){
  if(key=="children")return;
  if(key=="style"){
    if(!imports.setStyle)
      throw std::invalid_argument("Reconciler setStyle import required");
    imports.setStyle(node,value.value);
    if(node.yogaNode){
      const auto full=std::dynamic_pointer_cast<terminal_dom::FullLayoutNode>(
        node.yogaNode);
      if(!full)throw std::invalid_argument("Full Yoga layout import required");
      if(!imports.applyStyles)
        throw std::invalid_argument("Reconciler applyStyles import required");
      imports.applyStyles(*full,value.value);
    }
    return;
  }
  if(key=="textStyles"){
    node.textStyles=value.value;
    return;
  }
  if(terminal_events::isEventHandlerProp(key)){
    setEventHandler(node,key,value,imports.pointerHandlers);
    return;
  }
  if(!imports.setAttribute)
    throw std::invalid_argument("Reconciler setAttribute import required");
  imports.setAttribute(node,key,value.value?*value.value:
    Json(Json::value_t::discarded));
}
} // namespace batbox::terminal_reconciler
