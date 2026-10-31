#include "indigenous_yoga/Node.hpp"

namespace batbox::indigenous_yoga {
void Node::setDisplay(Display value){style.display=value;markDirty();}
Display Node::getDisplay()const{return style.display;}
void Node::setPositionType(PositionType value){
  style.positionType=value;markDirty();
}
void Node::setPosition(Edge edge,DimensionInput value){
  style.position[int(edge)]=parseDimension(value);
  _hasPosition=hasAnyDefinedEdge(style.position);
  markDirty();
}
void Node::setPositionPercent(Edge edge,double value){
  style.position[int(edge)]=percentValue(value);
  _hasPosition=true;markDirty();
}
void Node::setPositionAuto(Edge edge){
  style.position[int(edge)]=AUTO_VALUE;
  _hasPosition=true;markDirty();
}
void Node::setOverflow(Overflow value){style.overflow=value;markDirty();}
void Node::setDirection(Direction value){style.direction=value;markDirty();}
void Node::setBoxSizing(BoxSizing){}
void Node::setMargin(Edge edge,DimensionInput value){
  const auto parsed=parseDimension(value);
  style.margin[int(edge)]=parsed;
  if(parsed.unit==Unit::Auto)_hasAutoMargin=true;
  else _hasAutoMargin=hasAnyAutoEdge(style.margin);
  _hasMargin=_hasAutoMargin||hasAnyDefinedEdge(style.margin);
  markDirty();
}
void Node::setMarginPercent(Edge edge,double value){
  style.margin[int(edge)]=percentValue(value);
  _hasAutoMargin=hasAnyAutoEdge(style.margin);
  _hasMargin=true;markDirty();
}
void Node::setMarginAuto(Edge edge){
  style.margin[int(edge)]=AUTO_VALUE;
  _hasAutoMargin=true;_hasMargin=true;markDirty();
}
void Node::setPadding(Edge edge,DimensionInput value){
  style.padding[int(edge)]=parseDimension(value);
  _hasPadding=hasAnyDefinedEdge(style.padding);
  markDirty();
}
void Node::setPaddingPercent(Edge edge,double value){
  style.padding[int(edge)]=percentValue(value);
  _hasPadding=true;markDirty();
}
void Node::setBorder(Edge edge,std::optional<double> value){
  style.border[int(edge)]=value?pointValue(*value):UNDEFINED_VALUE;
  _hasBorder=hasAnyDefinedEdge(style.border);
  markDirty();
}
void Node::setGap(Gutter gutter,DimensionInput value){
  style.gap[int(gutter)]=parseDimension(value);markDirty();
}
void Node::setGapPercent(Gutter gutter,double value){
  style.gap[int(gutter)]=percentValue(value);markDirty();
}
} // namespace batbox::indigenous_yoga
