#include "terminal_render/RenderState.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::terminal_render {
void RenderState::resetLayoutShifted(){layoutShifted=false;}
bool RenderState::didLayoutShift()const{return layoutShifted;}
void RenderState::resetScrollHint(){
  scrollHint.reset();
  absoluteRectsPrev=std::move(absoluteRectsCur);
  absoluteRectsCur.clear();
}
std::optional<ScrollHint> RenderState::getScrollHint()const{
  return scrollHint;
}
void RenderState::resetScrollDrainNode(){scrollDrainNode.reset();}
terminal_dom::DOMElement* RenderState::getScrollDrainNode()const{
  return scrollDrainNode.get();
}
std::optional<FollowScroll> RenderState::consumeFollowScroll(){
  auto result=followScroll;
  followScroll.reset();
  return result;
}
bool isXtermJsHost(const std::function<std::optional<std::string>()>& termProgram,
    const std::function<bool()>& isXtermJs){
  if(!termProgram)
    throw std::invalid_argument("Terminal environment import required");
  if(termProgram()=="vscode")return true;
  if(!isXtermJs)
    throw std::invalid_argument("Terminal probe import required");
  return isXtermJs();
}
double drainAdaptive(terminal_dom::DOMElement& node,double pending,
    double innerHeight){
  using terminal_screen::typed_array::jsMax;
  const double sign=pending>0?1:-1;
  double abs=std::fabs(pending);
  double applied=0;
  if(abs>30){applied+=sign*(abs-30);abs=30;}
  const double step=abs<=5?abs:abs<12?2:3;
  applied+=sign*step;
  const double rem=abs-step;
  const double cap=jsMax(1,innerHeight-1);
  const double totalAbs=std::fabs(applied);
  if(totalAbs>cap){
    const double excess=totalAbs-cap;
    node.pendingScrollDelta=sign*(rem+excess);
    return sign*cap;
  }
  node.pendingScrollDelta=rem>0?std::optional<double>{sign*rem}:
    std::optional<double>{};
  return applied;
}
double drainProportional(terminal_dom::DOMElement& node,double pending,
    double innerHeight){
  using terminal_screen::typed_array::jsMax;
  using terminal_screen::typed_array::jsMin;
  const double abs=std::fabs(pending);
  const double cap=jsMax(1,innerHeight-1);
  const double shifted=static_cast<std::int32_t>(
    terminal_screen::typed_array::toInt32(abs*3))>>2;
  const double step=jsMin(cap,jsMax(4,shifted));
  if(abs<=step){
    node.pendingScrollDelta.reset();
    return pending;
  }
  const double applied=pending>0?step:-step;
  node.pendingScrollDelta=pending-applied;
  return applied;
}
} // namespace batbox::terminal_render
