#include "terminal_hit_test/HitTest.hpp"
#include <algorithm>

namespace batbox::terminal_hit_test {
namespace {
bool contains(const Hovered& values,const terminal_dom::DOMElement& node){
  return std::any_of(values.begin(),values.end(),[&](const auto& value){
    return value.get()==&node;
  });
}
}
void dispatchHover(terminal_dom::DOMElement& root,double col,double row,
    Hovered& hovered,const terminal_dom::NodeStash& stash,
    const PointerHandlerStore& handlers){
  Hovered next;
  auto* node=hitTest(root,col,row,stash);
  while(node){
    const auto* h=handlers.get(*node);
    if(h&&(h->onMouseEnter||h->onMouseLeave))
      next.push_back(std::static_pointer_cast<terminal_dom::DOMElement>(
        node->shared_from_this()));
    node=terminal_dom::parentElement(*node);
  }
  for(std::size_t i=0;i<hovered.size();){
    const auto old=hovered[i];
    if(contains(next,*old)){++i;continue;}
    hovered.erase(hovered.begin()+static_cast<std::ptrdiff_t>(i));
    if(old->parentNode){
      const auto* h=handlers.get(*old);
      if(h&&h->onMouseLeave){const auto handler=h->onMouseLeave;handler();}
    }
  }
  for(const auto& current:next){
    if(contains(hovered,*current))continue;
    hovered.push_back(current);
    const auto* h=handlers.get(*current);
    if(h&&h->onMouseEnter){const auto handler=h->onMouseEnter;handler();}
  }
}
} // namespace batbox::terminal_hit_test
