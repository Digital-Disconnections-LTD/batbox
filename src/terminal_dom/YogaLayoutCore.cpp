#include "terminal_dom/YogaLayoutNode.hpp"
#include "terminal_dom/YogaMaps.hpp"
#include <stdexcept>

namespace batbox::terminal_dom {
YogaLayoutNode::YogaLayoutNode(indigenous_yoga::NodeRef yoga):
  yoga_(std::move(yoga)){
  if(!yoga_)throw std::invalid_argument("Yoga node required");
}
namespace {
YogaLayoutNode& concrete(LayoutNode& node){
  auto* result=dynamic_cast<YogaLayoutNode*>(&node);
  if(!result)throw std::invalid_argument("YogaLayoutNode child required");
  return *result;
}
}
void YogaLayoutNode::insertChild(LayoutNode& child,std::size_t index){
  yoga_->insertChild(concrete(child).yoga_,static_cast<int>(index));
}
void YogaLayoutNode::removeChild(LayoutNode& child){
  yoga_->removeChild(concrete(child).yoga_);
}
std::size_t YogaLayoutNode::getChildCount()const{
  return static_cast<std::size_t>(yoga_->getChildCount());
}
FullLayoutRef YogaLayoutNode::getParent()const{
  auto* parent=yoga_->getParent();
  return parent?std::make_shared<YogaLayoutNode>(parent->shared_from_this()):
    FullLayoutRef{};
}
void YogaLayoutNode::calculateLayout(std::optional<double> width,
    std::optional<double>){
  yoga_->calculateLayout(width,{},indigenous_yoga::Direction::LTR);
}
void YogaLayoutNode::setMeasureFunc(MeasureFunction callback){
  yoga_->setMeasureFunc([callback=std::move(callback)](double width,
      indigenous_yoga::MeasureMode widthMode,double,
      indigenous_yoga::MeasureMode){
    const auto mode=widthMode==indigenous_yoga::MeasureMode::Exactly?
      LayoutMeasureMode::Exactly:
      widthMode==indigenous_yoga::MeasureMode::AtMost?
        LayoutMeasureMode::AtMost:LayoutMeasureMode::Undefined;
    const auto result=callback(width,mode);
    return indigenous_yoga::Size{result.width,result.height};
  });
}
void YogaLayoutNode::unsetMeasureFunc(){yoga_->unsetMeasureFunc();}
void YogaLayoutNode::markDirty(){yoga_->markDirty();}
double YogaLayoutNode::getComputedLeft()const{
  return yoga_->getComputedLeft();
}
double YogaLayoutNode::getComputedTop()const{
  return yoga_->getComputedTop();
}
double YogaLayoutNode::getComputedWidth()const{
  return yoga_->getComputedWidth();
}
double YogaLayoutNode::getComputedHeight()const{
  return yoga_->getComputedHeight();
}
double YogaLayoutNode::getComputedBorder(const std::string& edge)const{
  return yoga_->getComputedBorder(yoga_map::edge(edge));
}
double YogaLayoutNode::getComputedPadding(const std::string& edge)const{
  return yoga_->getComputedPadding(yoga_map::edge(edge));
}
LayoutDisplay YogaLayoutNode::getDisplay()const{
  return yoga_->getDisplay()==indigenous_yoga::Display::None?
    LayoutDisplay::None:LayoutDisplay::Flex;
}
void YogaLayoutNode::free(){yoga_->free();}
void YogaLayoutNode::freeRecursive(){yoga_->freeRecursive();}
FullLayoutRef createYogaLayoutNode(){
  return std::make_shared<YogaLayoutNode>(
    std::make_shared<indigenous_yoga::Node>());
}
FullLayoutRef createLayoutNode(){return createYogaLayoutNode();}
} // namespace batbox::terminal_dom
