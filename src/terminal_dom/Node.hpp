#pragma once
#include "runtime/JsonValue.hpp"
#include "terminal_events/Target.hpp"
#include "terminal_dom/LayoutPort.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::terminal_dom {
class FocusManager;

// Indigenous DOM nodes and elements. Layout implementation and
// renderer callbacks are bound by the owning application.
struct DOMNode:terminal_events::EventTarget,std::enable_shared_from_this<DOMNode> {
  explicit DOMNode(std::string name):nodeName(std::move(name)),
    style(std::make_shared<Json>(Json::object())){}
  virtual ~DOMNode()=default;
  std::string nodeName;
  std::shared_ptr<Json> style;
  LayoutRef yogaNode;
};
using NodeRef=std::shared_ptr<DOMNode>;

struct DOMElement:DOMNode {
  explicit DOMElement(std::string name):DOMNode(std::move(name)){}
  Json attributes=Json::object();
  std::vector<NodeRef> childNodes;
  std::shared_ptr<Json> textStyles;
  std::function<void()> onComputeLayout,onRender,onImmediateRender;
  std::optional<bool> hasRenderedContent,isHidden;
  bool dirty=false;
  std::optional<double> scrollTop,pendingScrollDelta,scrollClampMin,
    scrollClampMax,scrollHeight,scrollViewportHeight,scrollViewportTop;
  std::optional<bool> stickyScroll;
  struct ScrollAnchor {std::shared_ptr<DOMElement> el;double offset=0;};
  std::optional<ScrollAnchor> scrollAnchor;
  FocusManager* focusManager=nullptr;
  std::optional<std::vector<std::string>> debugOwnerChain;
};
using ElementRef=std::shared_ptr<DOMElement>;

struct TextNode:DOMNode {
  explicit TextNode(std::string value):DOMNode("#text"),nodeValue(std::move(value)){}
  std::string nodeValue;
};

// Focus-fixture tree binding. Production mutations use DOM operations, which
// also update layout nodes, pending clear rectangles and dirty state.
void bindFocusChild(DOMElement& parent,const NodeRef& child);
void unbindFocusChild(DOMElement& parent,const NodeRef& child);
DOMElement* parentElement(const DOMElement& node);
}
