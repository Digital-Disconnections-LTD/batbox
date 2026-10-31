#include "terminal_dom/YogaLayoutNode.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using namespace batbox;
namespace td=batbox::terminal_dom;
namespace {
void apply(td::FullLayoutNode& node,const Json& op){
  const auto name=op[0].get<std::string>();
  const auto string=[&](int index){return op[index].get<std::string>();};
  const auto number=[&](int index){return op[index].get<double>();};
  if(name=="setWidth")node.setWidth(number(1));
  else if(name=="setHeight")node.setHeight(number(1));
  else if(name=="setWidthPercent")node.setWidthPercent(number(1));
  else if(name=="setHeightPercent")node.setHeightPercent(number(1));
  else if(name=="setWidthAuto")node.setWidthAuto();
  else if(name=="setHeightAuto")node.setHeightAuto();
  else if(name=="setMinWidth")node.setMinWidth(number(1));
  else if(name=="setMaxWidth")node.setMaxWidth(number(1));
  else if(name=="setFlexDirection")node.setFlexDirection(string(1));
  else if(name=="setFlexGrow")node.setFlexGrow(number(1));
  else if(name=="setFlexShrink")node.setFlexShrink(number(1));
  else if(name=="setFlexBasis")node.setFlexBasis(number(1));
  else if(name=="setFlexWrap")node.setFlexWrap(string(1));
  else if(name=="setAlignItems")node.setAlignItems(string(1));
  else if(name=="setAlignSelf")node.setAlignSelf(string(1));
  else if(name=="setJustifyContent")node.setJustifyContent(string(1));
  else if(name=="setDisplay")node.setDisplay(string(1)=="flex"?
    td::LayoutDisplay::Flex:td::LayoutDisplay::None);
  else if(name=="setPositionType")node.setPositionType(string(1));
  else if(name=="setPosition")node.setPosition(string(1),number(2));
  else if(name=="setPositionPercent")
    node.setPositionPercent(string(1),number(2));
  else if(name=="setOverflow")node.setOverflow(string(1));
  else if(name=="setMargin")node.setMargin(string(1),number(2));
  else if(name=="setPadding")node.setPadding(string(1),number(2));
  else if(name=="setBorder")node.setBorder(string(1),number(2));
  else if(name=="setGap")node.setGap(string(1),number(2));
  else throw std::runtime_error("unknown method: "+name);
}
std::string mode(td::LayoutMeasureMode value){
  if(value==td::LayoutMeasureMode::Exactly)return "exactly";
  if(value==td::LayoutMeasureMode::AtMost)return "at-most";
  return "undefined";
}
Json snapshot(const td::FullLayoutNode& node){
  return {{"left",node.getComputedLeft()},{"top",node.getComputedTop()},
    {"width",node.getComputedWidth()},{"height",node.getComputedHeight()},
    {"borderLeft",node.getComputedBorder("left")},
    {"paddingTop",node.getComputedPadding("top")},
    {"display",node.getDisplay()==td::LayoutDisplay::None?"none":"flex"}};
}
Json run(const Json& spec){
  auto root=td::createLayoutNode(),child=td::createLayoutNode();
  Json trace=Json::array();
  if(spec.contains("rootOps"))for(const auto& op:spec["rootOps"])
    apply(*root,op);
  if(spec.contains("childOps"))for(const auto& op:spec["childOps"])
    apply(*child,op);
  if(spec.contains("measure")){
    const auto width=spec["measure"][0].get<double>();
    const auto height=spec["measure"][1].get<double>();
    child->setMeasureFunc([&,width,height](double available,
        td::LayoutMeasureMode measureMode){
      trace.push_back(Json::array({available,mode(measureMode)}));
      return td::MeasureSize{width,height};});
  }
  root->insertChild(*child,0);
  std::optional<double> width,height;
  if(spec.contains("width"))width=spec["width"].get<double>();
  if(spec.contains("height"))height=spec["height"].get<double>();
  root->calculateLayout(width,height);
  auto parent=child->getParent();
  const auto concreteParent=std::dynamic_pointer_cast<td::YogaLayoutNode>(parent);
  const auto concreteRoot=std::dynamic_pointer_cast<td::YogaLayoutNode>(root);
  return {{"root",snapshot(*root)},{"child",snapshot(*child)},
    {"trace",trace},{"childCount",root->getChildCount()},
    {"parentSameYoga",concreteParent->yoga()==concreteRoot->yoga()},
    {"parentDistinctWrapper",parent!=root}};
}
}
int main(){const std::string data((std::istreambuf_iterator<char>(std::cin)),{});
  const auto cases=value::parse(data);Json results=Json::array();
  for(const auto& item:cases)results.push_back(run(item));
  std::cout<<value::stringify(results)<<'\n';}
