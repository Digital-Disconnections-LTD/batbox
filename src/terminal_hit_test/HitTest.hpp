#pragma once
#include "terminal_dom/Focus.hpp"
#include "terminal_dom/NodeStash.hpp"
#include "terminal_events/Event.hpp"
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace batbox::terminal_hit_test {
struct PointerHandlers {
  std::function<void(terminal_events::ClickEvent&)> onClick;
  std::function<void()> onMouseEnter,onMouseLeave;
};
class PointerHandlerStore {
public:
  void set(const terminal_dom::ElementRef& node,PointerHandlers handlers);
  void setClick(const terminal_dom::ElementRef& node,
    std::function<void(terminal_events::ClickEvent&)> handler);
  void setEnter(const terminal_dom::ElementRef& node,
    std::function<void()> handler);
  void setLeave(const terminal_dom::ElementRef& node,
    std::function<void()> handler);
  const PointerHandlers* get(const terminal_dom::DOMElement& node)const;
  void erase(const terminal_dom::DOMElement& node);
private:
  using Key=std::weak_ptr<terminal_dom::DOMElement>;
  mutable std::map<Key,PointerHandlers,std::owner_less<Key>> handlers_;
};
using Hovered=std::vector<terminal_dom::ElementRef>;

terminal_dom::DOMElement* hitTest(terminal_dom::DOMElement& node,
  double col,double row,const terminal_dom::NodeStash& stash);
bool dispatchClick(terminal_dom::DOMElement& root,double col,double row,
  bool cellIsBlank,const terminal_dom::NodeStash& stash,
  const PointerHandlerStore& handlers);
void dispatchHover(terminal_dom::DOMElement& root,double col,double row,
  Hovered& hovered,const terminal_dom::NodeStash& stash,
  const PointerHandlerStore& handlers);
} // namespace batbox::terminal_hit_test
