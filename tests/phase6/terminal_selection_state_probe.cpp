#include "terminal_selection/Selection.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
namespace sel=batbox::terminal_selection;
namespace {
Json point(const sel::PointRef& p){
  return p?Json{{"col",p->col},{"row",p->row}}:Json(nullptr);
}
Json span(const sel::SpanRef& s){
  return s?Json{{"lo",point(s->lo)},{"hi",point(s->hi)},
    {"kind",s->kind==sel::SpanKind::Word?"word":"line"}}:Json(nullptr);
}
template<class T>Json array(const std::shared_ptr<std::vector<T>>& values){
  Json result=Json::array();for(const auto& value:*values)result.push_back(value);
  return result;
}
Json snapshot(const char* kind,const sel::SelectionState& s,Json extra={}){
  Json bounds=nullptr;
  if(const auto b=sel::selectionBounds(s))
    bounds=Json{{"start",point(b->start)},{"end",point(b->end)}};
  Json row={{"kind",kind},{"anchor",point(s.anchor)},
    {"focus",point(s.focus)},{"dragging",s.isDragging},
    {"span",span(s.anchorSpan)},{"above",array(s.scrolledOffAbove)},
    {"below",array(s.scrolledOffBelow)},
    {"aboveSW",array(s.scrolledOffAboveSW)},
    {"belowSW",array(s.scrolledOffBelowSW)},
    {"virtualAnchor",s.virtualAnchorRow?Json(*s.virtualAnchorRow):Json(nullptr)},
    {"virtualFocus",s.virtualFocusRow?Json(*s.virtualFocusRow):Json(nullptr)},
    {"alt",s.lastPressHadAlt},{"has",sel::hasSelection(s)},
    {"bounds",bounds},
    {"cells",Json::array({sel::isCellSelected(s,0,0),
      sel::isCellSelected(s,2,3),sel::isCellSelected(s,4,5)})}};
  if(extra.is_object())for(auto it=extra.begin();it!=extra.end();++it)
    row[it.key()]=it.value();
  return row;
}
}
int main(){
  Json rows=Json::array();
  auto s=sel::createSelectionState();rows.push_back(snapshot("create",s));
  sel::startSelection(s,2,3);rows.push_back(snapshot("start",s));
  sel::updateSelection(s,2,3);rows.push_back(snapshot("sameMotion",s));
  sel::updateSelection(s,4,5);rows.push_back(snapshot("drag",s));
  sel::finishSelection(s);rows.push_back(snapshot("finish",s));
  sel::moveFocus(s,1,4);rows.push_back(snapshot("moveFocus",s));
  sel::clearSelection(s);rows.push_back(snapshot("clear",s));
  s=sel::createSelectionState();sel::startSelection(s,2,3);
  sel::updateSelection(s,4,5);
  s.scrolledOffAbove->insert(s.scrolledOffAbove->end(),{"old1","old2"});
  s.scrolledOffAboveSW->insert(s.scrolledOffAboveSW->end(),{false,true});
  sel::shiftSelection(s,-5,0,5,10);
  rows.push_back(snapshot("shiftAbove",s));
  sel::shiftSelection(s,2,0,5,10);
  rows.push_back(snapshot("reverseAbove",s));
  s=sel::createSelectionState();sel::startSelection(s,1,2);
  sel::updateSelection(s,4,4);
  s.scrolledOffBelow->insert(s.scrolledOffBelow->end(),{"x","y"});
  s.scrolledOffBelowSW->insert(s.scrolledOffBelowSW->end(),{false,true});
  sel::shiftSelection(s,1,0,4,10);
  rows.push_back(snapshot("shiftBelow",s));
  sel::shiftSelection(s,-1,0,4,10);
  rows.push_back(snapshot("reverseBelow",s));
  sel::shiftSelection(s,-10,0,4,10);
  rows.push_back(snapshot("overshootClear",s));
  s=sel::createSelectionState();sel::startSelection(s,2,2);
  sel::updateSelection(s,4,3);
  sel::shiftAnchor(s,-4,0,5);rows.push_back(snapshot("anchorClamp",s));
  sel::shiftSelectionForFollow(s,1,0,5);
  rows.push_back(snapshot("followReverse",s));
  const bool cleared=sel::shiftSelectionForFollow(s,-5,0,5);
  rows.push_back(snapshot("followClear",s,Json{{"cleared",cleared}}));
  std::cout<<rows.dump()<<'\n';
}
