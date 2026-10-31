#include "terminal_dom/style/StyleInternal.hpp"

namespace batbox::terminal_dom::style::detail {
namespace {
bool named(const Json* value,const char* name){
  return value&&string(*value)&&asString(*value)==name;
}
void dimension(FullLayoutNode& node,const Json* value,
    void(FullLayoutNode::*point)(double),
    void(FullLayoutNode::*percent)(double),
    void(FullLayoutNode::*automatic)()){
  if(!value)return;
  if(number(*value))(node.*point)(asNumber(*value));
  else if(string(*value))(node.*percent)(parseInt10(asString(*value)));
  else (node.*automatic)();
}
void boundedDimension(FullLayoutNode& node,const Json* value,
    void(FullLayoutNode::*point)(double),
    void(FullLayoutNode::*percent)(double)){
  if(!value)return;
  if(string(*value))(node.*percent)(parseInt10(asString(*value)));
  else (node.*point)(numberOr(value,0));
}
}
void applyFlexStyles(FullLayoutNode& node,const Styles& style){
  if(const auto* value=field(style,"flexGrow"))
    node.setFlexGrow(numberOr(value,0));
  if(const auto* value=field(style,"flexShrink"))
    node.setFlexShrink(number(*value)?asNumber(*value):1);
  if(const auto* value=field(style,"flexWrap")){
    if(named(value,"nowrap"))node.setFlexWrap("nowrap");
    if(named(value,"wrap"))node.setFlexWrap("wrap");
    if(named(value,"wrap-reverse"))node.setFlexWrap("wrap-reverse");
  }
  if(const auto* value=field(style,"flexDirection")){
    if(named(value,"row"))node.setFlexDirection("row");
    if(named(value,"row-reverse"))node.setFlexDirection("row-reverse");
    if(named(value,"column"))node.setFlexDirection("column");
    if(named(value,"column-reverse"))node.setFlexDirection("column-reverse");
  }
  if(const auto* value=field(style,"flexBasis")){
    if(number(*value))node.setFlexBasis(asNumber(*value));
    else if(string(*value))
      node.setFlexBasisPercent(parseInt10(asString(*value)));
    else node.setFlexBasis(NaN);
  }
  if(const auto* value=field(style,"alignItems")){
    if(named(value,"stretch")||!truthy(*value))node.setAlignItems("stretch");
    if(named(value,"flex-start"))node.setAlignItems("flex-start");
    if(named(value,"center"))node.setAlignItems("center");
    if(named(value,"flex-end"))node.setAlignItems("flex-end");
  }
  if(const auto* value=field(style,"alignSelf")){
    if(named(value,"auto")||!truthy(*value))node.setAlignSelf("auto");
    if(named(value,"flex-start"))node.setAlignSelf("flex-start");
    if(named(value,"center"))node.setAlignSelf("center");
    if(named(value,"flex-end"))node.setAlignSelf("flex-end");
  }
  if(const auto* value=field(style,"justifyContent")){
    if(named(value,"flex-start")||!truthy(*value))
      node.setJustifyContent("flex-start");
    if(named(value,"center"))node.setJustifyContent("center");
    if(named(value,"flex-end"))node.setJustifyContent("flex-end");
    if(named(value,"space-between"))node.setJustifyContent("space-between");
    if(named(value,"space-around"))node.setJustifyContent("space-around");
    if(named(value,"space-evenly"))node.setJustifyContent("space-evenly");
  }
}
void applyDimensionStyles(FullLayoutNode& node,const Styles& style){
  dimension(node,field(style,"width"),&FullLayoutNode::setWidth,
    &FullLayoutNode::setWidthPercent,&FullLayoutNode::setWidthAuto);
  dimension(node,field(style,"height"),&FullLayoutNode::setHeight,
    &FullLayoutNode::setHeightPercent,&FullLayoutNode::setHeightAuto);
  boundedDimension(node,field(style,"minWidth"),
    &FullLayoutNode::setMinWidth,&FullLayoutNode::setMinWidthPercent);
  boundedDimension(node,field(style,"minHeight"),
    &FullLayoutNode::setMinHeight,&FullLayoutNode::setMinHeightPercent);
  boundedDimension(node,field(style,"maxWidth"),
    &FullLayoutNode::setMaxWidth,&FullLayoutNode::setMaxWidthPercent);
  boundedDimension(node,field(style,"maxHeight"),
    &FullLayoutNode::setMaxHeight,&FullLayoutNode::setMaxHeightPercent);
}
void applyDisplayStyles(FullLayoutNode& node,const Styles& style){
  if(const auto* value=field(style,"display"))
    node.setDisplay(named(value,"flex")?LayoutDisplay::Flex:LayoutDisplay::None);
}
} // namespace batbox::terminal_dom::style::detail
