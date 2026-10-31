#include "terminal_dom/YogaLayoutNode.hpp"
#include "terminal_dom/YogaMaps.hpp"

namespace batbox::terminal_dom {
void YogaLayoutNode::setWidth(double value){yoga_->setWidth(value);}
void YogaLayoutNode::setWidthPercent(double value){
  yoga_->setWidthPercent(value);}
void YogaLayoutNode::setWidthAuto(){yoga_->setWidthAuto();}
void YogaLayoutNode::setHeight(double value){yoga_->setHeight(value);}
void YogaLayoutNode::setHeightPercent(double value){
  yoga_->setHeightPercent(value);}
void YogaLayoutNode::setHeightAuto(){yoga_->setHeightAuto();}
void YogaLayoutNode::setMinWidth(double value){yoga_->setMinWidth(value);}
void YogaLayoutNode::setMinWidthPercent(double value){
  yoga_->setMinWidthPercent(value);}
void YogaLayoutNode::setMinHeight(double value){yoga_->setMinHeight(value);}
void YogaLayoutNode::setMinHeightPercent(double value){
  yoga_->setMinHeightPercent(value);}
void YogaLayoutNode::setMaxWidth(double value){yoga_->setMaxWidth(value);}
void YogaLayoutNode::setMaxWidthPercent(double value){
  yoga_->setMaxWidthPercent(value);}
void YogaLayoutNode::setMaxHeight(double value){yoga_->setMaxHeight(value);}
void YogaLayoutNode::setMaxHeightPercent(double value){
  yoga_->setMaxHeightPercent(value);}
void YogaLayoutNode::setFlexDirection(const std::string& value){
  yoga_->setFlexDirection(yoga_map::flexDirection(value));}
void YogaLayoutNode::setFlexGrow(double value){yoga_->setFlexGrow(value);}
void YogaLayoutNode::setFlexShrink(double value){yoga_->setFlexShrink(value);}
void YogaLayoutNode::setFlexBasis(double value){yoga_->setFlexBasis(value);}
void YogaLayoutNode::setFlexBasisPercent(double value){
  yoga_->setFlexBasisPercent(value);}
void YogaLayoutNode::setFlexWrap(const std::string& value){
  yoga_->setFlexWrap(yoga_map::wrap(value));}
void YogaLayoutNode::setAlignItems(const std::string& value){
  yoga_->setAlignItems(yoga_map::align(value));}
void YogaLayoutNode::setAlignSelf(const std::string& value){
  yoga_->setAlignSelf(yoga_map::align(value));}
void YogaLayoutNode::setJustifyContent(const std::string& value){
  yoga_->setJustifyContent(yoga_map::justify(value));}
void YogaLayoutNode::setDisplay(LayoutDisplay value){
  yoga_->setDisplay(value==LayoutDisplay::Flex?
    indigenous_yoga::Display::Flex:indigenous_yoga::Display::None);}
void YogaLayoutNode::setPositionType(const std::string& value){
  yoga_->setPositionType(yoga_map::positionType(value));}
void YogaLayoutNode::setPosition(const std::string& edge,double value){
  yoga_->setPosition(yoga_map::edge(edge),value);}
void YogaLayoutNode::setPositionPercent(const std::string& edge,
    double value){
  yoga_->setPositionPercent(yoga_map::edge(edge),value);}
void YogaLayoutNode::setOverflow(const std::string& value){
  yoga_->setOverflow(yoga_map::overflow(value));}
void YogaLayoutNode::setMargin(const std::string& edge,double value){
  yoga_->setMargin(yoga_map::edge(edge),value);}
void YogaLayoutNode::setPadding(const std::string& edge,double value){
  yoga_->setPadding(yoga_map::edge(edge),value);}
void YogaLayoutNode::setBorder(const std::string& edge,double value){
  yoga_->setBorder(yoga_map::edge(edge),value);}
void YogaLayoutNode::setGap(const std::string& gutter,double value){
  yoga_->setGap(yoga_map::gutter(gutter),value);}
} // namespace batbox::terminal_dom
