#include "indigenous_yoga/Types.hpp"

namespace batbox::indigenous_yoga {
namespace {
constexpr int LEFT=0,TOP=1,RIGHT=2,BOTTOM=3;
Value fallback(const StyleEdges& edges,int physical){
  auto value=edges[physical];
  if(value.unit==Unit::Undefined)
    value=edges[physical==LEFT||physical==RIGHT?
      int(Edge::Horizontal):int(Edge::Vertical)];
  if(value.unit==Unit::Undefined)value=edges[int(Edge::All)];
  if(value.unit==Unit::Undefined){
    if(physical==LEFT)value=edges[int(Edge::Start)];
    if(physical==RIGHT)value=edges[int(Edge::End)];
  }
  return value;
}
}
double resolveEdge(const StyleEdges& edges,int physical,double ownerSize,
    bool allowAuto){
  const auto value=fallback(edges,physical);
  if(value.unit==Unit::Undefined)return 0;
  if(value.unit==Unit::Auto)return allowAuto?NaN:0;
  return resolveValue(value,ownerSize);
}
Value resolveEdgeRaw(const StyleEdges& edges,int physical){
  return fallback(edges,physical);
}
bool isMarginAuto(const StyleEdges& edges,int physical){
  return fallback(edges,physical).unit==Unit::Auto;
}
bool hasAnyAutoEdge(const StyleEdges& edges){
  for(int i=0;i<9;++i)if(edges[i].unit==Unit::Auto)return true;
  return false;
}
bool hasAnyDefinedEdge(const StyleEdges& edges){
  for(int i=0;i<9;++i)if(edges[i].unit!=Unit::Undefined)return true;
  return false;
}
void resolveEdges4Into(const StyleEdges& edges,double ownerSize,Edges& out){
  const auto horizontal=edges[int(Edge::Horizontal)];
  const auto vertical=edges[int(Edge::Vertical)];
  const auto all=edges[int(Edge::All)];
  const auto start=edges[int(Edge::Start)];
  const auto end=edges[int(Edge::End)];
  const auto denom=std::isnan(ownerSize)?NaN:ownerSize/100;
  Value value=edges[LEFT];
  if(value.unit==Unit::Undefined)value=horizontal;
  if(value.unit==Unit::Undefined)value=all;
  if(value.unit==Unit::Undefined)value=start;
  out[LEFT]=value.unit==Unit::Point?value.value:
    value.unit==Unit::Percent?value.value*denom:0;
  value=edges[TOP];
  if(value.unit==Unit::Undefined)value=vertical;
  if(value.unit==Unit::Undefined)value=all;
  out[TOP]=value.unit==Unit::Point?value.value:
    value.unit==Unit::Percent?value.value*denom:0;
  value=edges[RIGHT];
  if(value.unit==Unit::Undefined)value=horizontal;
  if(value.unit==Unit::Undefined)value=all;
  if(value.unit==Unit::Undefined)value=end;
  out[RIGHT]=value.unit==Unit::Point?value.value:
    value.unit==Unit::Percent?value.value*denom:0;
  value=edges[BOTTOM];
  if(value.unit==Unit::Undefined)value=vertical;
  if(value.unit==Unit::Undefined)value=all;
  out[BOTTOM]=value.unit==Unit::Point?value.value:
    value.unit==Unit::Percent?value.value*denom:0;
}
int physicalEdge(Edge edge){
  switch(edge){
    case Edge::Left:case Edge::Start:return LEFT;
    case Edge::Top:return TOP;
    case Edge::Right:case Edge::End:return RIGHT;
    case Edge::Bottom:return BOTTOM;
    default:return LEFT;
  }
}
} // namespace batbox::indigenous_yoga
