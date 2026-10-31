#include "terminal_dom/Dom.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::terminal_dom {
namespace {
DOMElement* parent(DOMNode& node){
  return static_cast<DOMElement*>(node.parentNode);
}
void collectRemovedRects(DOMElement& host,DOMNode& removed,
    NodeStash& stash,bool underAbsolute=false){
  if(removed.nodeName=="#text")return;
  auto& elem=static_cast<DOMElement&>(removed);
  const bool absolute=underAbsolute||
    (elem.style&&elem.style->is_object()&&
      elem.style->value("position",Json())==Json("absolute"));
  if(const auto stashed=stash.get(elem)){
    stash.addPendingClear(host,stashed,absolute);
    stash.erase(elem);
  }
  for(const auto& child:elem.childNodes)
    collectRemovedRects(host,*child,stash,absolute);
}
}
void appendChildNode(DOMElement& node,const NodeRef& child,
    NodeStash& stash){
  if(!child)throw std::invalid_argument("DOM child required");
  if(child->parentNode)removeChildNode(*parent(*child),child,stash);
  child->parentNode=&node;
  node.childNodes.push_back(child);
  if(child->yogaNode&&node.yogaNode)
    node.yogaNode->insertChild(*child->yogaNode,node.yogaNode->getChildCount());
  markDirty(&node);
}
void insertBeforeNode(DOMElement& node,const NodeRef& child,
    const NodeRef& before,NodeStash& stash){
  if(!child||!before)throw std::invalid_argument("DOM child required");
  if(child->parentNode)removeChildNode(*parent(*child),child,stash);
  child->parentNode=&node;
  const auto found=std::find_if(node.childNodes.begin(),node.childNodes.end(),
    [&](const NodeRef& current){return current.get()==before.get();});
  if(found!=node.childNodes.end()){
    const auto index=static_cast<std::size_t>(found-node.childNodes.begin());
    std::size_t yogaIndex=0;
    if(child->yogaNode&&node.yogaNode)
      for(std::size_t i=0;i<index;++i)
        if(node.childNodes[i]&&node.childNodes[i]->yogaNode)++yogaIndex;
    node.childNodes.insert(node.childNodes.begin()+
      static_cast<std::ptrdiff_t>(index),child);
    if(child->yogaNode&&node.yogaNode)
      node.yogaNode->insertChild(*child->yogaNode,yogaIndex);
    markDirty(&node);
    return;
  }
  node.childNodes.push_back(child);
  if(child->yogaNode&&node.yogaNode)
    node.yogaNode->insertChild(*child->yogaNode,node.yogaNode->getChildCount());
  markDirty(&node);
}
void removeChildNode(DOMElement& node,const NodeRef& child,NodeStash& stash){
  if(!child)throw std::invalid_argument("DOM child required");
  if(child->yogaNode){
    auto* currentParent=parent(*child);
    if(currentParent&&currentParent->yogaNode)
      currentParent->yogaNode->removeChild(*child->yogaNode);
  }
  collectRemovedRects(node,*child,stash);
  child->parentNode=nullptr;
  const auto found=std::find_if(node.childNodes.begin(),node.childNodes.end(),
    [&](const NodeRef& current){return current.get()==child.get();});
  if(found!=node.childNodes.end())node.childNodes.erase(found);
  markDirty(&node);
}
} // namespace batbox::terminal_dom
