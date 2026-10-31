#include "terminal_selection/Selection.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace batbox::terminal_selection {
namespace {
PointRef makePoint(double col,double row){
  return std::make_shared<Point>(Point{col,row});
}
double clamp(double value,double min,double max){
  return terminal_screen::clamp(value,min,max);
}
template<class T>void setLength(std::vector<T>& values,double length){
  if(!std::isfinite(length)||length<0||std::trunc(length)!=length||
      length>4294967295.0)
    throw std::range_error("Invalid array length");
  values.resize(static_cast<std::size_t>(length));
}
template<class T>void spliceFront(std::vector<T>& values,double count){
  const auto n=std::min(values.size(),static_cast<std::size_t>(
    std::max(0.0,std::trunc(count))));
  values.erase(values.begin(),values.begin()+
    static_cast<std::ptrdiff_t>(n));
}
template<class T>std::vector<T> sliceTail(const std::vector<T>& values,
    double length){
  const auto n=std::min(values.size(),static_cast<std::size_t>(
    std::max(0.0,std::trunc(length))));
  return {values.end()-static_cast<std::ptrdiff_t>(n),values.end()};
}
template<class T>std::vector<T> sliceHead(const std::vector<T>& values,
    double length){
  const auto n=std::min(values.size(),static_cast<std::size_t>(
    std::max(0.0,std::trunc(length))));
  return {values.begin(),values.begin()+static_cast<std::ptrdiff_t>(n)};
}
PointRef edgeShift(const PointRef& p,double row,double minRow,
    double maxRow,double width){
  if(row<minRow)return makePoint(0,minRow);
  if(row>maxRow)return makePoint(width-1,maxRow);
  return makePoint(p->col,row);
}
SpanRef shiftSpanEdges(const SpanRef& span,double dRow,double minRow,
    double maxRow,double width){
  if(!span)return {};
  return std::make_shared<AnchorSpan>(AnchorSpan{
    edgeShift(span->lo,span->lo->row+dRow,minRow,maxRow,width),
    edgeShift(span->hi,span->hi->row+dRow,minRow,maxRow,width),
    span->kind});
}
SpanRef shiftSpanClamp(const SpanRef& span,double dRow,double minRow,
    double maxRow){
  if(!span)return {};
  return std::make_shared<AnchorSpan>(AnchorSpan{
    makePoint(span->lo->col,clamp(span->lo->row+dRow,minRow,maxRow)),
    makePoint(span->hi->col,clamp(span->hi->row+dRow,minRow,maxRow)),
    span->kind});
}
}
void shiftSelection(SelectionState& s,double dRow,double minRow,
    double maxRow,double width){
  if(!s.anchor||!s.focus)return;
  const double vAnchor=s.virtualAnchorRow.value_or(s.anchor->row)+dRow;
  const double vFocus=s.virtualFocusRow.value_or(s.focus->row)+dRow;
  if((vAnchor<minRow&&vFocus<minRow)||
      (vAnchor>maxRow&&vFocus>maxRow)){
    clearSelection(s);return;
  }
  const double oldMin=std::min(s.virtualAnchorRow.value_or(s.anchor->row),
    s.virtualFocusRow.value_or(s.focus->row));
  const double oldMax=std::max(s.virtualAnchorRow.value_or(s.anchor->row),
    s.virtualFocusRow.value_or(s.focus->row));
  const double oldAboveDebt=std::max(0.0,minRow-oldMin);
  const double oldBelowDebt=std::max(0.0,oldMax-maxRow);
  const double newAboveDebt=std::max(0.0,minRow-std::min(vAnchor,vFocus));
  const double newBelowDebt=std::max(0.0,std::max(vAnchor,vFocus)-maxRow);
  if(newAboveDebt<oldAboveDebt){
    const double drop=oldAboveDebt-newAboveDebt;
    setLength(*s.scrolledOffAbove,s.scrolledOffAbove->size()-drop);
    setLength(*s.scrolledOffAboveSW,s.scrolledOffAbove->size());
  }
  if(newBelowDebt<oldBelowDebt){
    const double drop=oldBelowDebt-newBelowDebt;
    spliceFront(*s.scrolledOffBelow,drop);
    spliceFront(*s.scrolledOffBelowSW,drop);
  }
  if(s.scrolledOffAbove->size()>newAboveDebt){
    s.scrolledOffAbove=std::make_shared<std::vector<std::string>>(
      newAboveDebt>0?sliceTail(*s.scrolledOffAbove,newAboveDebt):
        std::vector<std::string>{});
    s.scrolledOffAboveSW=std::make_shared<std::vector<bool>>(
      newAboveDebt>0?sliceTail(*s.scrolledOffAboveSW,newAboveDebt):
        std::vector<bool>{});
  }
  if(s.scrolledOffBelow->size()>newBelowDebt){
    s.scrolledOffBelow=std::make_shared<std::vector<std::string>>(
      sliceHead(*s.scrolledOffBelow,newBelowDebt));
    s.scrolledOffBelowSW=std::make_shared<std::vector<bool>>(
      sliceHead(*s.scrolledOffBelowSW,newBelowDebt));
  }
  s.anchor=edgeShift(s.anchor,vAnchor,minRow,maxRow,width);
  s.focus=edgeShift(s.focus,vFocus,minRow,maxRow,width);
  s.virtualAnchorRow=(vAnchor<minRow||vAnchor>maxRow)?
    std::optional<double>(vAnchor):std::nullopt;
  s.virtualFocusRow=(vFocus<minRow||vFocus>maxRow)?
    std::optional<double>(vFocus):std::nullopt;
  if(s.anchorSpan)s.anchorSpan=shiftSpanEdges(
    s.anchorSpan,dRow,minRow,maxRow,width);
}
void shiftAnchor(SelectionState& s,double dRow,double minRow,double maxRow){
  if(!s.anchor)return;
  const double raw=s.virtualAnchorRow.value_or(s.anchor->row)+dRow;
  s.anchor=makePoint(s.anchor->col,clamp(raw,minRow,maxRow));
  s.virtualAnchorRow=(raw<minRow||raw>maxRow)?
    std::optional<double>(raw):std::nullopt;
  if(s.anchorSpan)
    s.anchorSpan=shiftSpanClamp(s.anchorSpan,dRow,minRow,maxRow);
}
bool shiftSelectionForFollow(SelectionState& s,double dRow,double minRow,
    double maxRow){
  if(!s.anchor)return false;
  const double rawAnchor=s.virtualAnchorRow.value_or(s.anchor->row)+dRow;
  const std::optional<double> rawFocus=s.focus?
    std::optional<double>(s.virtualFocusRow.value_or(s.focus->row)+dRow):
    std::nullopt;
  if(rawAnchor<minRow&&rawFocus&&*rawFocus<minRow){
    clearSelection(s);return true;
  }
  s.anchor=makePoint(s.anchor->col,clamp(rawAnchor,minRow,maxRow));
  if(s.focus&&rawFocus)
    s.focus=makePoint(s.focus->col,clamp(*rawFocus,minRow,maxRow));
  s.virtualAnchorRow=(rawAnchor<minRow||rawAnchor>maxRow)?
    std::optional<double>(rawAnchor):std::nullopt;
  s.virtualFocusRow=(rawFocus&&(*rawFocus<minRow||*rawFocus>maxRow))?
    rawFocus:std::nullopt;
  if(s.anchorSpan)
    s.anchorSpan=shiftSpanClamp(s.anchorSpan,dRow,minRow,maxRow);
  return false;
}
} // namespace batbox::terminal_selection
