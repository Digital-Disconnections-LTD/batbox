#include "indigenous_yoga/Node.hpp"

namespace batbox::indigenous_yoga {
FlexDirection Node::getFlexDirection()const{return style.flexDirection;}
Justify Node::getJustifyContent()const{return style.justifyContent;}
Align Node::getAlignItems()const{return style.alignItems;}
Align Node::getAlignSelf()const{return style.alignSelf;}
Align Node::getAlignContent()const{return style.alignContent;}
double Node::getFlexGrow()const{return style.flexGrow;}
double Node::getFlexShrink()const{return style.flexShrink;}
Value Node::getFlexBasis()const{return style.flexBasis;}
Wrap Node::getFlexWrap()const{return style.flexWrap;}
Value Node::getWidth()const{return style.width;}
Value Node::getHeight()const{return style.height;}
Overflow Node::getOverflow()const{return style.overflow;}
PositionType Node::getPositionType()const{return style.positionType;}
Direction Node::getDirection()const{return style.direction;}
} // namespace batbox::indigenous_yoga
