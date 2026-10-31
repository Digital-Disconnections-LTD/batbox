#pragma once
#include "terminal_render/RenderNode.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::terminal_render::detail {
inline const Json* field(const terminal_dom::DOMNode& node,
    const char* name){
  if(!node.style||!node.style->is_object())return nullptr;
  const auto found=node.style->find(name);
  return found==node.style->end()?nullptr:&*found;
}
inline const Json* nullish(const terminal_dom::DOMNode& node,
    const char* first,const char* fallback){
  const auto value=field(node,first);
  return value&&!value->is_null()&&!value->is_discarded()?
    value:field(node,fallback);
}
inline std::optional<std::string> string(const Json* value){
  if(!value||value->is_null()||value->is_discarded())return {};
  if(value->is_string())return value->get<std::string>();
  throw std::invalid_argument("Render style string required");
}
inline std::optional<std::string> string(const terminal_dom::DOMNode& node,
    const char* name){return string(field(node,name));}
inline bool truthy(const Json* value){return value&&value::truthy(*value);}
inline bool truthy(const terminal_dom::DOMNode& node,const char* name){
  return truthy(field(node,name));
}
inline terminal_dom::FullLayoutRef fullLayout(
    const terminal_dom::DOMNode& node){
  if(!node.yogaNode)return {};
  const auto result=std::dynamic_pointer_cast<terminal_dom::FullLayoutNode>(
    node.yogaNode);
  if(!result)throw std::invalid_argument("Full Yoga layout import required");
  return result;
}
inline terminal_dom::ElementRef element(const terminal_dom::NodeRef& node){
  return std::dynamic_pointer_cast<terminal_dom::DOMElement>(node);
}
inline std::shared_ptr<terminal_screen::Rectangle> rect(double x,double y,
    double width,double height){
  return std::make_shared<terminal_screen::Rectangle>(
    terminal_screen::Rectangle{x,y,width,height});
}
inline std::string repeatJoined(const std::string& value,double count){
  if(!std::isfinite(count)||count<0||std::trunc(count)!=count)
    throw std::range_error("Invalid array length");
  std::string result;
  for(std::size_t i=0;i<static_cast<std::size_t>(count);i++){
    if(i)result+='\n';
    result+=value;
  }
  return result;
}
} // namespace batbox::terminal_render::detail
