#include "terminal_dom/Dom.hpp"
#include <stdexcept>

namespace batbox::terminal_dom {
ElementRef createNode(const std::string& nodeName,
    const DomServicesRef& services){
  const bool needsYogaNode=nodeName!="ink-virtual-text"&&
    nodeName!="ink-link"&&nodeName!="ink-progress";
  if(needsYogaNode&&(!services||!services->createLayoutNode))
    throw std::invalid_argument("DOM layout factory required");
  if(nodeName=="ink-text"&&(!services->expandTabs||
      !services->measureText||!services->wrapText))
    throw std::invalid_argument("DOM text measurement services required");
  auto node=std::make_shared<DOMElement>(nodeName);
  if(needsYogaNode){
    node->yogaNode=services->createLayoutNode();
    if(!node->yogaNode)throw std::invalid_argument("DOM layout factory returned null");
  }
  if(nodeName=="ink-text"){
    std::weak_ptr<DOMElement> weak=node;
    node->yogaNode->setMeasureFunc([weak,services](double width,
        LayoutMeasureMode mode){
      const auto current=weak.lock();
      if(!current)throw std::runtime_error("DOM text measure node expired");
      return measureTextNode(*current,width,mode,*services);
    });
  }else if(nodeName=="ink-raw-ansi"){
    std::weak_ptr<DOMElement> weak=node;
    node->yogaNode->setMeasureFunc([weak](double,LayoutMeasureMode){
      const auto current=weak.lock();
      if(!current)throw std::runtime_error("DOM raw ANSI measure node expired");
      return measureRawAnsiNode(*current);
    });
  }
  return node;
}
std::shared_ptr<TextNode> createTextNode(const std::string& text){
  auto node=std::make_shared<TextNode>(text);
  setTextNodeValue(*node,text);
  return node;
}
} // namespace batbox::terminal_dom
