#include "terminal_hit_test/HitTest.hpp"

namespace batbox::terminal_hit_test {
terminal_dom::DOMElement* hitTest(terminal_dom::DOMElement& node,
    double col,double row,const terminal_dom::NodeStash& stash){
  const auto rect=stash.get(node);
  if(!rect)return nullptr;
  if(col<rect->x||col>=rect->x+rect->width||
      row<rect->y||row>=rect->y+rect->height)return nullptr;
  for(auto it=node.childNodes.rbegin();it!=node.childNodes.rend();++it){
    const auto& child=*it;
    if(child->nodeName=="#text")continue;
    if(auto* hit=hitTest(static_cast<terminal_dom::DOMElement&>(*child),
        col,row,stash))return hit;
  }
  return &node;
}
bool dispatchClick(terminal_dom::DOMElement& root,double col,double row,
    bool cellIsBlank,const terminal_dom::NodeStash& stash,
    const PointerHandlerStore& handlers){
  auto* target=hitTest(root,col,row,stash);
  if(!target)return false;
  if(root.focusManager){
    auto* focusTarget=target;
    while(focusTarget){
      if(focusTarget->attributes.is_object()){
        const auto found=focusTarget->attributes.find("tabIndex");
        if(found!=focusTarget->attributes.end()&&found->is_number()){
          root.focusManager->handleClickFocus(
            std::static_pointer_cast<terminal_dom::DOMElement>(
              focusTarget->shared_from_this()));
          break;
        }
      }
      focusTarget=terminal_dom::parentElement(*focusTarget);
    }
  }
  terminal_events::ClickEvent event(col,row,cellIsBlank);
  bool handled=false;
  while(target){
    const auto* h=handlers.get(*target);
    if(h&&h->onClick){
      const auto handler=h->onClick;
      handled=true;
      const auto rect=stash.get(*target);
      if(rect){event.localCol=col-rect->x;event.localRow=row-rect->y;}
      handler(event);
      if(event.didStopImmediatePropagation())return true;
    }
    target=terminal_dom::parentElement(*target);
  }
  return handled;
}
} // namespace batbox::terminal_hit_test
