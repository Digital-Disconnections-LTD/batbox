#include "indigenous_yoga/Node.hpp"
#include <cmath>

namespace batbox::indigenous_yoga {
void Node::setFlexDirection(FlexDirection value){
  style.flexDirection=value;markDirty();
}
void Node::setFlexGrow(std::optional<double> value){
  style.flexGrow=value.value_or(0);markDirty();
}
void Node::setFlexShrink(std::optional<double> value){
  style.flexShrink=value.value_or(0);markDirty();
}
void Node::setFlex(std::optional<double> value){
  if(!value||std::isnan(*value)){
    style.flexGrow=0;style.flexShrink=0;
  }else if(*value>0){
    style.flexGrow=*value;style.flexShrink=1;
    style.flexBasis=pointValue(0);
  }else if(*value<0){
    style.flexGrow=0;style.flexShrink=-*value;
  }else{
    style.flexGrow=0;style.flexShrink=0;
  }
  markDirty();
}
void Node::setFlexBasis(DimensionInput value){
  style.flexBasis=parseDimension(value);markDirty();
}
void Node::setFlexBasisPercent(double value){
  style.flexBasis=percentValue(value);markDirty();
}
void Node::setFlexBasisAuto(){style.flexBasis=AUTO_VALUE;markDirty();}
void Node::setFlexWrap(Wrap value){style.flexWrap=value;markDirty();}
void Node::setAlignItems(Align value){style.alignItems=value;markDirty();}
void Node::setAlignSelf(Align value){style.alignSelf=value;markDirty();}
void Node::setAlignContent(Align value){
  style.alignContent=value;markDirty();
}
void Node::setJustifyContent(Justify value){
  style.justifyContent=value;markDirty();
}
} // namespace batbox::indigenous_yoga
