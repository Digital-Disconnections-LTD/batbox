#include "indigenous_yoga/Node.hpp"

namespace batbox::indigenous_yoga {
void Node::setWidth(DimensionInput value){
  style.width=parseDimension(value);markDirty();
}
void Node::setWidthPercent(double value){
  style.width=percentValue(value);markDirty();
}
void Node::setWidthAuto(){style.width=AUTO_VALUE;markDirty();}
void Node::setHeight(DimensionInput value){
  style.height=parseDimension(value);markDirty();
}
void Node::setHeightPercent(double value){
  style.height=percentValue(value);markDirty();
}
void Node::setHeightAuto(){style.height=AUTO_VALUE;markDirty();}
void Node::setMinWidth(DimensionInput value){
  style.minWidth=parseDimension(value);markDirty();
}
void Node::setMinWidthPercent(double value){
  style.minWidth=percentValue(value);markDirty();
}
void Node::setMinHeight(DimensionInput value){
  style.minHeight=parseDimension(value);markDirty();
}
void Node::setMinHeightPercent(double value){
  style.minHeight=percentValue(value);markDirty();
}
void Node::setMaxWidth(DimensionInput value){
  style.maxWidth=parseDimension(value);markDirty();
}
void Node::setMaxWidthPercent(double value){
  style.maxWidth=percentValue(value);markDirty();
}
void Node::setMaxHeight(DimensionInput value){
  style.maxHeight=parseDimension(value);markDirty();
}
void Node::setMaxHeightPercent(double value){
  style.maxHeight=percentValue(value);markDirty();
}
} // namespace batbox::indigenous_yoga
