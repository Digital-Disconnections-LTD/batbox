#include "terminal_dom/Dom.hpp"

namespace batbox::terminal_dom {
void markDirty(DOMNode* node){
  auto* current=node;
  bool markedYoga=false;
  while(current){
    if(current->nodeName!="#text"){
      auto& element=static_cast<DOMElement&>(*current);
      element.dirty=true;
      if(!markedYoga&&
          (element.nodeName=="ink-text"||
           element.nodeName=="ink-raw-ansi")&&element.yogaNode){
        element.yogaNode->markDirty();
        markedYoga=true;
      }
    }
    current=static_cast<DOMNode*>(current->parentNode);
  }
}
void scheduleRenderFrom(DOMNode* node){
  auto* current=node;
  while(current&&current->parentNode)
    current=static_cast<DOMNode*>(current->parentNode);
  if(current&&current->nodeName!="#text"){
    auto& root=static_cast<DOMElement&>(*current);
    if(root.onRender)root.onRender();
  }
}
void clearYogaNodeReferences(DOMNode& node){
  if(node.nodeName!="#text"){
    auto& element=static_cast<DOMElement&>(node);
    for(const auto& child:element.childNodes)
      clearYogaNodeReferences(*child);
  }
  node.yogaNode.reset();
}
std::vector<std::string> findOwnerChainAtRow(DOMElement& root,double y){
  std::vector<std::string> best;
  const auto walk=[&](const auto& self,DOMElement& node,double offsetY)->void{
    const auto yoga=node.yogaNode;
    if(!yoga||yoga->getDisplay()==LayoutDisplay::None)return;
    const auto top=offsetY+yoga->getComputedTop();
    const auto height=yoga->getComputedHeight();
    if(y<top||y>=top+height)return;
    if(node.debugOwnerChain)best=*node.debugOwnerChain;
    for(const auto& child:node.childNodes)
      if(child->nodeName!="#text")
        self(self,static_cast<DOMElement&>(*child),top);
  };
  walk(walk,root,0);
  return best;
}
} // namespace batbox::terminal_dom
