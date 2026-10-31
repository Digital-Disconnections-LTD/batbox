#include "terminal_reconciler/HostConfig.hpp"
#include "terminal_dom/FullLayoutPort.hpp"
#include <stdexcept>

namespace batbox::terminal_reconciler {
namespace {
Json styleSnapshot(const PropsRef& props){
  Json result=Json::object();
  for(const auto& key:props->keys()){
    const auto value=props->get(key);
    result[key]=value?*value:Json{{"$undefined",true}};
  }
  return result;
}
}
void HostConfig::commitUpdate(terminal_dom::DOMElement& node,
    const PropsBundle& oldProps,const PropsBundle& newProps){
  const auto props=diff(oldProps.values,newProps.values);
  const auto style=diff(oldProps.style,newProps.style);
  if(props){
    for(const auto& key:props->keys()){
      HostPropValue value{props->get(key),{}};
      if(key=="style"){
        if(!services_.props.setStyle)
          throw std::invalid_argument("Reconciler setStyle import required");
        services_.props.setStyle(node,value.value);
        continue;
      }
      if(key=="textStyles"){
        if(!services_.setTextStyles)
          throw std::invalid_argument("Reconciler setTextStyles import required");
        services_.setTextStyles(node,value.value);
        continue;
      }
      if(terminal_events::isEventHandlerProp(key)){
        const auto found=newProps.handlers.find(key);
        if(found!=newProps.handlers.end())value.handler=found->second;
        const auto click=newProps.clickHandlers.find(key);
        if(click!=newProps.clickHandlers.end())value.onClick=click->second;
        const auto hover=newProps.hoverHandlers.find(key);
        if(hover!=newProps.hoverHandlers.end())value.onHover=hover->second;
        setEventHandler(node,key,value,services_.props.pointerHandlers);
        continue;
      }
      if(!services_.props.setAttribute)
        throw std::invalid_argument("Reconciler setAttribute import required");
      services_.props.setAttribute(node,key,
        value.value?*value.value:Json(Json::value_t::discarded));
    }
  }
  if(style&&node.yogaNode){
    const auto yoga=std::dynamic_pointer_cast<terminal_dom::FullLayoutNode>(
      node.yogaNode);
    if(!yoga)throw std::invalid_argument("Full Yoga layout import required");
    if(!services_.applyStyleDiff)
      throw std::invalid_argument("Reconciler applyStyles import required");
    const auto changed=styleSnapshot(style);
    const auto resolved=newProps.values?newProps.values->get("style"):
      batbox::value::ValueRef{};
    services_.applyStyleDiff(*yoga,changed,resolved.get());
  }
}
} // namespace batbox::terminal_reconciler
