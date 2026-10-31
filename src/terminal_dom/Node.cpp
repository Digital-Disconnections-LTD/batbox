#include "terminal_dom/Node.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::terminal_dom {
DOMElement* parentElement(const DOMElement& node){
  // DOM projection invariant: every element parent is another DOMElement.
  return static_cast<DOMElement*>(node.parentNode);
}
void unbindFocusChild(DOMElement& parent,const NodeRef& child){
  if(!child)throw std::invalid_argument("Focus DOM child required");
  auto& children=parent.childNodes;
  const auto found=std::find_if(children.begin(),children.end(),
    [&](const NodeRef& value){return value.get()==child.get();});
  if(found!=children.end())children.erase(found);
  if(child->parentNode==&parent)child->parentNode=nullptr;
}
void bindFocusChild(DOMElement& parent,const NodeRef& child){
  if(!child)throw std::invalid_argument("Focus DOM child required");
  if(child->parentNode){
    auto* old=static_cast<DOMElement*>(child->parentNode);
    unbindFocusChild(*old,child);
  }
  child->parentNode=&parent;
  parent.childNodes.push_back(child);
}
}
