#pragma once
#include "terminal_dom/Node.hpp"
#include "terminal_events/Event.hpp"
#include <functional>
#include <optional>
#include <vector>

namespace batbox::terminal_dom {
using FocusDispatch=std::function<bool(DOMElement&,terminal_events::FocusEvent&)>;
class FocusManager {
public:
  FocusManager(FocusDispatch dispatchFocusEvent,terminal_events::MonotonicNow now);
  ElementRef activeElement;
  void focus(const ElementRef& node);
  void blur();
  void handleNodeRemoved(const ElementRef& node,DOMElement& root);
  void handleAutoFocus(const ElementRef& node);
  void handleClickFocus(const ElementRef& node);
  void enable();
  void disable();
  void focusNext(DOMElement& root);
  void focusPrevious(DOMElement& root);
  std::size_t stackSize()const{return focusStack_.size();}
private:
  void moveFocus(int direction,DOMElement& root);
  FocusDispatch dispatchFocusEvent_;
  terminal_events::MonotonicNow now_;
  bool enabled_=true;
  std::vector<ElementRef> focusStack_;
};

std::vector<ElementRef> collectTabbable(DOMElement& root);
bool isInTree(const DOMElement& node,const DOMElement& root);
DOMElement& getRootNode(DOMElement& node);
FocusManager& getFocusManager(DOMElement& node);
}
