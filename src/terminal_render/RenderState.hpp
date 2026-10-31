#pragma once
#include "terminal_dom/NodeStash.hpp"
#include "terminal_screen/Geometry.hpp"
#include <functional>
#include <optional>
#include <vector>

namespace batbox::terminal_render {
struct ScrollHint {double top=0,bottom=0,delta=0;};
struct FollowScroll {double delta=0,viewportTop=0,viewportBottom=0;};
class RenderState {
public:
  explicit RenderState(terminal_dom::NodeStash& stash):nodeStash(stash){}
  void resetLayoutShifted();
  bool didLayoutShift()const;
  void resetScrollHint();
  std::optional<ScrollHint> getScrollHint()const;
  void resetScrollDrainNode();
  terminal_dom::DOMElement* getScrollDrainNode()const;
  std::optional<FollowScroll> consumeFollowScroll();
  terminal_dom::NodeStash& nodeStash;
  bool layoutShifted=false;
  std::optional<ScrollHint> scrollHint;
  std::vector<terminal_dom::StashedRef> absoluteRectsPrev,
    absoluteRectsCur;
  terminal_dom::ElementRef scrollDrainNode;
  std::optional<FollowScroll> followScroll;
};
bool isXtermJsHost(const std::function<std::optional<std::string>()>& termProgram,
  const std::function<bool()>& isXtermJs);
double drainAdaptive(terminal_dom::DOMElement& node,double pending,
  double innerHeight);
double drainProportional(terminal_dom::DOMElement& node,double pending,
  double innerHeight);
} // namespace batbox::terminal_render
