#include "terminal_hit_test/HitTest.hpp"
#include <iterator>

namespace batbox::terminal_hit_test {
namespace {
std::weak_ptr<terminal_dom::DOMElement> key(
    const terminal_dom::DOMElement& node){
  return std::static_pointer_cast<terminal_dom::DOMElement>(
    const_cast<terminal_dom::DOMElement&>(node).shared_from_this());
}
}
void PointerHandlerStore::set(const terminal_dom::ElementRef& node,
    PointerHandlers handlers){
  for(auto it=handlers_.begin();it!=handlers_.end();)
    it=it->first.expired()?handlers_.erase(it):std::next(it);
  handlers_[node]=std::move(handlers);
}
void PointerHandlerStore::setClick(const terminal_dom::ElementRef& node,
    std::function<void(terminal_events::ClickEvent&)> handler){
  handlers_[node].onClick=std::move(handler);
}
void PointerHandlerStore::setEnter(const terminal_dom::ElementRef& node,
    std::function<void()> handler){
  handlers_[node].onMouseEnter=std::move(handler);
}
void PointerHandlerStore::setLeave(const terminal_dom::ElementRef& node,
    std::function<void()> handler){
  handlers_[node].onMouseLeave=std::move(handler);
}
const PointerHandlers* PointerHandlerStore::get(
    const terminal_dom::DOMElement& node)const{
  for(auto it=handlers_.begin();it!=handlers_.end();)
    it=it->first.expired()?handlers_.erase(it):std::next(it);
  const auto found=handlers_.find(key(node));
  return found==handlers_.end()?nullptr:&found->second;
}
void PointerHandlerStore::erase(const terminal_dom::DOMElement& node){
  handlers_.erase(key(node));
}
} // namespace batbox::terminal_hit_test
