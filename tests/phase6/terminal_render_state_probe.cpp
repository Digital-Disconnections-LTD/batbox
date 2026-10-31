#include "terminal_render/RenderState.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
namespace rend=batbox::terminal_render;
namespace dom=batbox::terminal_dom;
namespace {
Json optionalNumber(std::optional<double> value){
  return value?Json(*value):Json(nullptr);
}
Json hint(std::optional<rend::ScrollHint> value){
  return value?Json{{"top",value->top},{"bottom",value->bottom},
    {"delta",value->delta}}:Json(nullptr);
}
Json follow(std::optional<rend::FollowScroll> value){
  return value?Json{{"delta",value->delta},
    {"viewportTop",value->viewportTop},
    {"viewportBottom",value->viewportBottom}}:Json(nullptr);
}
}
int main(){
  Json rows=Json::array();
  for(const std::string term:{"vscode","xterm",""}){
    int probeCalls=0;
    const bool host=rend::isXtermJsHost([&](){return term;},
      [&](){probeCalls++;return true;});
    rows.push_back(Json{{"kind","host"},{"term",term},{"host",host},
      {"probeCalls",probeCalls}});
  }
  const std::vector<std::tuple<std::string,double,double>> cases{
    {"adaptiveClick",2,10},{"adaptiveMedium",8,10},
    {"adaptiveHigh",18,10},{"adaptiveSnap",40,10},
    {"adaptiveCapped",-40,2},{"adaptiveZero",0,10},
    {"proportionalSmall",3,10},{"proportionalBig",20,10},
    {"proportionalNegative",-20,4},{"proportionalOverflow",1e12,10}};
  for(const auto& [kind,pending,height]:cases){
    dom::DOMElement node("ink-box");node.pendingScrollDelta=pending;
    const double applied=kind.starts_with("adaptive")?
      rend::drainAdaptive(node,pending,height):
      rend::drainProportional(node,pending,height);
    rows.push_back(Json{{"kind",kind},{"applied",applied},
      {"pending",optionalNumber(node.pendingScrollDelta)},
      {"hasPending",node.pendingScrollDelta.has_value()}});
  }
  dom::NodeStash stash;rend::RenderState state(stash);
  auto rect=std::make_shared<dom::StashedLayout>();rect->x=1;
  auto node=std::make_shared<dom::DOMElement>("ink-box");
  state.layoutShifted=true;state.scrollHint=rend::ScrollHint{1,4,2};
  state.absoluteRectsCur.push_back(rect);
  state.scrollDrainNode=node;
  state.followScroll=rend::FollowScroll{3,2,5};
  const auto first=state.consumeFollowScroll();
  const auto again=state.consumeFollowScroll();
  rows.push_back(Json{{"kind","stateBefore"},
    {"shifted",state.didLayoutShift()},{"hint",hint(state.getScrollHint())},
    {"drainSame",state.getScrollDrainNode()==node.get()},
    {"follow",follow(first)},{"followAgain",follow(again)}});
  state.resetLayoutShifted();state.resetScrollHint();
  state.resetScrollDrainNode();
  rows.push_back(Json{{"kind","stateAfter"},
    {"shifted",state.didLayoutShift()},{"hint",hint(state.getScrollHint())},
    {"drain",nullptr},{"prevSame",state.absoluteRectsPrev[0]==rect},
    {"currentLength",state.absoluteRectsCur.size()}});
  std::cout<<rows.dump()<<'\n';
}
