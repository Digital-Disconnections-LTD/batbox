#pragma once
#include "terminal_reconciler/HostLeaves.hpp"
#include "terminal_reconciler/HostProps.hpp"
#include <map>

namespace batbox::terminal_reconciler {
struct HostContext {bool isInsideText=false;};
using HostContextRef=std::shared_ptr<HostContext>;
struct PropsBundle {
  PropsRef values;
  PropsRef style;
  std::map<std::string,terminal_events::HandlerValue> handlers;
  std::map<std::string,
    std::function<void(terminal_events::ClickEvent&)>> clickHandlers;
  std::map<std::string,std::function<void()>> hoverHandlers;
};
struct HostConfigServices {
  terminal_dom::DomServicesRef dom;
  terminal_dom::NodeStash* stash=nullptr;
  HostPropImports props;
  std::function<void(terminal_dom::DOMElement&,
    const batbox::value::ValueRef&)> setTextStyles;
  std::function<void(terminal_dom::FullLayoutNode&,const Json&,
    const Json*)> applyStyleDiff;
  DebugRepaints* debugRepaints=nullptr;
  bool commitLogEnabled=false;
  std::function<void()> recordCreatedNode;
};
class HostConfig {
public:
  explicit HostConfig(HostConfigServices services);
  HostContextRef getRootHostContext()const;
  HostContextRef getChildHostContext(const HostContextRef& parent,
    const std::string& type)const;
  terminal_dom::ElementRef createInstance(const std::string& originalType,
    const PropsBundle& props,const HostContextRef& context,
    const OwnerFiberRef& internalHandle={});
  std::shared_ptr<terminal_dom::TextNode> createTextInstance(
    const std::string& text,const HostContextRef& context);
  void hideTextInstance(terminal_dom::TextNode& node);
  void unhideTextInstance(terminal_dom::TextNode& node,
    const std::string& text);
  void hideInstance(terminal_dom::DOMElement& node);
  void unhideInstance(terminal_dom::DOMElement& node);
  void appendInitialChild(terminal_dom::DOMElement& node,
    const terminal_dom::NodeRef& child);
  void appendChild(terminal_dom::DOMElement& node,
    const terminal_dom::NodeRef& child);
  void insertBefore(terminal_dom::DOMElement& node,
    const terminal_dom::NodeRef& child,
    const terminal_dom::NodeRef& before);
  bool finalizeInitialChildren(const PropsBundle& props)const;
  void commitMount(terminal_dom::DOMElement& node);
  void removeChildFromContainer(terminal_dom::DOMElement& node,
    const terminal_dom::NodeRef& removeNode);
  void removeChild(terminal_dom::DOMElement& node,
    const terminal_dom::NodeRef& removeNode);
  void commitUpdate(terminal_dom::DOMElement& node,
    const PropsBundle& oldProps,const PropsBundle& newProps);
  void commitTextUpdate(terminal_dom::TextNode& node,
    const std::string& newText);
private:
  HostConfigServices services_;
};
HostConfigServices indigenousHostConfigServices(
  terminal_dom::DomServicesRef dom,terminal_dom::NodeStash& stash,
  DebugRepaints& debugRepaints,
  terminal_hit_test::PointerHandlerStore* pointerHandlers=nullptr);
} // namespace batbox::terminal_reconciler
