#include "terminal_render/StyledText.hpp"
#include <stdexcept>

namespace batbox::terminal_render {
StyledSegmentsRef squashTextNodesToSegments(
    const terminal_dom::DOMElement& node,StylesRef inheritedStyles,
    std::optional<std::string> inheritedHyperlink,StyledSegmentsRef out){
  if(!inheritedStyles)inheritedStyles=std::make_shared<Json>(Json::object());
  if(!out)out=std::make_shared<StyledSegments>();
  auto mergedStyles=inheritedStyles;
  if(node.textStyles&&value::truthy(*node.textStyles)){
    auto next=std::make_shared<Json>(*inheritedStyles);
    if(!next->is_object())*next=Json::object();
    if(node.textStyles->is_object())
      for(auto it=node.textStyles->begin();it!=node.textStyles->end();++it)
        (*next)[it.key()]=it.value();
    mergedStyles=std::move(next);
  }
  for(const auto& childNode:node.childNodes){
    if(!childNode)continue;
    if(childNode->nodeName=="#text"){
      const auto text=std::dynamic_pointer_cast<terminal_dom::TextNode>(childNode);
      if(!text)throw std::invalid_argument("Text node identity required");
      if(!text->nodeValue.empty())
        out->push_back(StyledSegment{text->nodeValue,mergedStyles,
          inheritedHyperlink});
    }else if(childNode->nodeName=="ink-text"||
        childNode->nodeName=="ink-virtual-text"){
      const auto element=std::dynamic_pointer_cast<terminal_dom::DOMElement>(childNode);
      if(!element)throw std::invalid_argument("Element identity required");
      squashTextNodesToSegments(*element,mergedStyles,inheritedHyperlink,out);
    }else if(childNode->nodeName=="ink-link"){
      const auto element=std::dynamic_pointer_cast<terminal_dom::DOMElement>(childNode);
      if(!element)throw std::invalid_argument("Link element identity required");
      auto href=inheritedHyperlink;
      if(element->attributes.is_object()){
        const auto it=element->attributes.find("href");
        if(it!=element->attributes.end()&&it->is_string()&&
            !it->get<std::string>().empty())href=it->get<std::string>();
      }
      squashTextNodesToSegments(*element,mergedStyles,href,out);
    }
  }
  return out;
}
std::vector<std::size_t> buildCharToSegmentMap(
    const StyledSegments& segments){
  std::vector<std::size_t> map;
  for(std::size_t i=0;i<segments.size();i++){
    const auto len=value::stringLength(segments[i].text);
    for(std::size_t j=0;j<len;j++)map.push_back(i);
  }
  return map;
}
std::string wrapWithOsc8Link(const std::string& text,
    const std::string& url){
  const std::string osc="\x1b]",bel="\x07";
  return osc+"8;;"+url+bel+text+osc+"8;;"+bel;
}
} // namespace batbox::terminal_render
