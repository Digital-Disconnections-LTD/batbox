#include "terminal_selection/Selection.hpp"

namespace batbox::terminal_selection {
namespace {
PointRef point(double col,double row){
  return std::make_shared<Point>(Point{col,row});
}
void reset(SelectionState& s){
  s.anchor.reset();s.focus.reset();s.isDragging=false;
  s.anchorSpan.reset();
  s.scrolledOffAbove=std::make_shared<std::vector<std::string>>();
  s.scrolledOffBelow=std::make_shared<std::vector<std::string>>();
  s.scrolledOffAboveSW=std::make_shared<std::vector<bool>>();
  s.scrolledOffBelowSW=std::make_shared<std::vector<bool>>();
  s.virtualAnchorRow.reset();s.virtualFocusRow.reset();
  s.lastPressHadAlt=false;
}
}
SelectionState createSelectionState(){return {};}
void startSelection(SelectionState& s,double col,double row){
  reset(s);s.anchor=point(col,row);s.isDragging=true;
}
void updateSelection(SelectionState& s,double col,double row){
  if(!s.isDragging)return;
  if(!s.focus&&s.anchor&&s.anchor->col==col&&s.anchor->row==row)return;
  s.focus=point(col,row);
}
void finishSelection(SelectionState& s){s.isDragging=false;}
void clearSelection(SelectionState& s){reset(s);}
void moveFocus(SelectionState& s,double col,double row){
  if(!s.focus)return;
  s.anchorSpan.reset();s.focus=point(col,row);s.virtualFocusRow.reset();
}
int comparePoints(const Point& a,const Point& b){
  if(a.row!=b.row)return a.row<b.row?-1:1;
  if(a.col!=b.col)return a.col<b.col?-1:1;
  return 0;
}
bool hasSelection(const SelectionState& s){return bool(s.anchor&&s.focus);}
std::optional<Bounds> selectionBounds(const SelectionState& s){
  if(!s.anchor||!s.focus)return {};
  if(comparePoints(*s.anchor,*s.focus)<=0)
    return Bounds{s.anchor,s.focus};
  return Bounds{s.focus,s.anchor};
}
bool isCellSelected(const SelectionState& s,double col,double row){
  const auto b=selectionBounds(s);
  if(!b)return false;
  const auto& start=*b->start;const auto& end=*b->end;
  if(row<start.row||row>end.row)return false;
  if(row==start.row&&col<start.col)return false;
  if(row==end.row&&col>end.col)return false;
  return true;
}
} // namespace batbox::terminal_selection
