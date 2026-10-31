#include "terminal_dom/Dom.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace batbox::terminal_dom {
std::string squashTextNodes(const DOMElement& node){
  std::string text;
  for(const auto& child:node.childNodes){
    if(!child)continue;
    if(child->nodeName=="#text")
      text+=static_cast<const TextNode&>(*child).nodeValue;
    else if(child->nodeName=="ink-text"||
        child->nodeName=="ink-virtual-text"||
        child->nodeName=="ink-link")
      text+=squashTextNodes(static_cast<const DOMElement&>(*child));
  }
  return text;
}
MeasureSize measureTextNode(const DOMNode& node,double width,
    LayoutMeasureMode widthMode,const DomServices& services){
  if(!services.expandTabs||!services.measureText||!services.wrapText)
    throw std::invalid_argument("DOM text measurement services required");
  const auto rawText=node.nodeName=="#text"?
    static_cast<const TextNode&>(node).nodeValue:
    squashTextNodes(static_cast<const DOMElement&>(node));
  const auto text=services.expandTabs(rawText);
  const auto dimensions=services.measureText(text,width);
  if(dimensions.width<=width)return {dimensions.width,dimensions.height};
  if(dimensions.width>=1&&width>0&&width<1)
    return {dimensions.width,dimensions.height};
  if(text.find('\n')!=std::string::npos&&
      widthMode==LayoutMeasureMode::Undefined){
    const auto effectiveWidth=std::isnan(width)||
      std::isnan(dimensions.width)?
      std::numeric_limits<double>::quiet_NaN():
      std::max(width,dimensions.width);
    const auto result=services.measureText(text,effectiveWidth);
    return {result.width,result.height};
  }
  std::string textWrap="wrap";
  if(node.style&&node.style->is_object()){
    const auto found=node.style->find("textWrap");
    if(found!=node.style->end()&&!found->is_null())
      textWrap=found->get<std::string>();
  }
  const auto wrapped=services.wrapText(text,width,textWrap);
  const auto result=services.measureText(wrapped,width);
  return {result.width,result.height};
}
MeasureSize measureRawAnsiNode(const DOMElement& node){
  MeasureSize result;
  const auto width=node.attributes.find("rawWidth");
  const auto height=node.attributes.find("rawHeight");
  if(width!=node.attributes.end()&&width->is_number())
    result.width=width->get<double>();
  if(height!=node.attributes.end()&&height->is_number())
    result.height=height->get<double>();
  return result;
}
} // namespace batbox::terminal_dom
