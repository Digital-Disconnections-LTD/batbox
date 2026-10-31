#pragma once
#include "terminal_dom/Dom.hpp"
#include "runtime/SharedRecord.hpp"
#include "terminal_dom/FullLayoutPort.hpp"
#include "terminal_events/Dispatcher.hpp"
#include "terminal_hit_test/HitTest.hpp"
#include <functional>

namespace batbox::terminal_reconciler {
struct HostPropValue {
  batbox::value::ValueRef value;
  std::optional<terminal_events::HandlerValue> handler;
  std::function<void(terminal_events::ClickEvent&)> onClick;
  std::function<void()> onHover;
};
struct HostPropImports {
  std::function<void(terminal_dom::DOMElement&,
    const batbox::value::ValueRef&)> setStyle;
  std::function<void(terminal_dom::FullLayoutNode&,
    const batbox::value::ValueRef&)> applyStyles;
  std::function<void(terminal_dom::DOMElement&,const std::string&,
    const Json&)> setAttribute;
  terminal_hit_test::PointerHandlerStore* pointerHandlers=nullptr;
};
void setEventHandler(terminal_dom::DOMElement& node,
  const std::string& key,const HostPropValue& value,
  terminal_hit_test::PointerHandlerStore* pointerHandlers=nullptr);
void applyProp(terminal_dom::DOMElement& node,const std::string& key,
  const HostPropValue& value,const HostPropImports& imports);
HostPropImports indigenousHostPropImports(
  terminal_hit_test::PointerHandlerStore* pointerHandlers=nullptr);
} // namespace batbox::terminal_reconciler
