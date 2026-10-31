#pragma once
#include "terminal_dom/NodeStash.hpp"

namespace batbox::terminal_dom {
struct DomServices {
  LayoutFactory createLayoutNode;
  std::function<std::string(const std::string&)> expandTabs;
  std::function<terminal_screen::Size(const std::string&,double)> measureText;
  std::function<std::string(const std::string&,double,const std::string&)> wrapText;
};
using DomServicesRef=std::shared_ptr<DomServices>;

ElementRef createNode(const std::string& nodeName,const DomServicesRef& services);
std::shared_ptr<TextNode> createTextNode(const std::string& text);
void appendChildNode(DOMElement& node,const NodeRef& child,NodeStash& stash);
void insertBeforeNode(DOMElement& node,const NodeRef& child,
  const NodeRef& before,NodeStash& stash);
void removeChildNode(DOMElement& node,const NodeRef& child,NodeStash& stash);
void setAttribute(DOMElement& node,const std::string& key,const Json& value);
void setStyle(DOMNode& node,const std::shared_ptr<Json>& style);
void setTextStyles(DOMElement& node,const std::shared_ptr<Json>& textStyles);
bool shallowEqual(const std::shared_ptr<Json>& a,const std::shared_ptr<Json>& b);
std::string squashTextNodes(const DOMElement& node);
MeasureSize measureTextNode(const DOMNode& node,double width,
  LayoutMeasureMode widthMode,const DomServices& services);
MeasureSize measureRawAnsiNode(const DOMElement& node);
void markDirty(DOMNode* node);
void scheduleRenderFrom(DOMNode* node);
void setTextNodeValue(TextNode& node,const std::string& text);
void setTextNodeValueDynamic(TextNode& node,const Json& text);
void clearYogaNodeReferences(DOMNode& node);
std::vector<std::string> findOwnerChainAtRow(DOMElement& root,double y);
} // namespace batbox::terminal_dom
