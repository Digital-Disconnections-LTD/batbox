#include "indigenous_yoga/Node.hpp"
#include <algorithm>

namespace batbox::indigenous_yoga {
Node::Node(ConfigRef selected):style(defaultStyle()),
  config(selected?std::move(selected):defaultConfig()){
  incrementLiveNodes();
}
void Node::insertChild(NodeRef child,int index){
  child->parent=this;
  const auto count=static_cast<int>(children.size());
  const auto position=index<0?std::max(0,count+index):std::min(index,count);
  children.insert(children.begin()+position,std::move(child));
  markDirty();
}
void Node::removeChild(const NodeRef& child){
  const auto found=std::find(children.begin(),children.end(),child);
  if(found!=children.end()){
    children.erase(found);
    child->parent=nullptr;
    markDirty();
  }
}
NodeRef Node::getChild(int index)const{
  return index>=0&&index<static_cast<int>(children.size())?
    children[index]:NodeRef{};
}
int Node::getChildCount()const{return static_cast<int>(children.size());}
Node* Node::getParent()const{return parent;}
void Node::free(){
  parent=nullptr;children.clear();measureFunc={};
  _cIn={};_cOut={};_stashAllocated=false;decrementLiveNodes();
}
void Node::freeRecursive(){
  for(const auto& child:children)child->freeRecursive();
  free();
}
void Node::reset(){
  style=defaultStyle();children.clear();parent=nullptr;measureFunc={};
  isDirty_=true;_hasAutoMargin=false;_hasPosition=false;
  _hasPadding=false;_hasBorder=false;_hasMargin=false;
  _hasL=false;_hasM=false;_cN=0;_cWr=0;_fbBasis=NaN;
}
void Node::markDirty(){
  isDirty_=true;
  if(parent&&!parent->isDirty_)parent->markDirty();
}
bool Node::isDirty()const{return isDirty_;}
bool Node::hasNewLayout()const{return true;}
void Node::markLayoutSeen(){}
void Node::setMeasureFunc(MeasureFunction callback){
  measureFunc=std::move(callback);markDirty();
}
void Node::unsetMeasureFunc(){measureFunc={};markDirty();}
double Node::getComputedLeft()const{return layout.left;}
double Node::getComputedTop()const{return layout.top;}
double Node::getComputedWidth()const{return layout.width;}
double Node::getComputedHeight()const{return layout.height;}
double Node::getComputedRight()const{
  return parent?parent->layout.width-layout.left-layout.width:0;
}
double Node::getComputedBottom()const{
  return parent?parent->layout.height-layout.top-layout.height:0;
}
ComputedLayout Node::getComputedLayout()const{
  return {layout.left,layout.top,getComputedRight(),
    getComputedBottom(),layout.width,layout.height};
}
double Node::getComputedBorder(Edge edge)const{
  return layout.border[physicalEdge(edge)];
}
double Node::getComputedPadding(Edge edge)const{
  return layout.padding[physicalEdge(edge)];
}
double Node::getComputedMargin(Edge edge)const{
  return layout.margin[physicalEdge(edge)];
}
void Node::copyStyle(const Node&){}
void Node::setDirtiedFunc(){}
void Node::unsetDirtiedFunc(){}
void Node::setIsReferenceBaseline(bool value){
  isReferenceBaseline_=value;markDirty();
}
bool Node::isReferenceBaseline()const{return isReferenceBaseline_;}
void Node::setAspectRatio(std::optional<double>){}
double Node::getAspectRatio()const{return NaN;}
void Node::setAlwaysFormsContainingBlock(bool){}
void Node::calculateLayout(std::optional<double> ownerWidth,
    std::optional<double> ownerHeight,std::optional<Direction>){
  resetCountersForLayout();
  const auto width=ownerWidth.value_or(NaN);
  const auto height=ownerHeight.value_or(NaN);
  layoutNode(*this,width,height,
    isDefined(width)?MeasureMode::Exactly:MeasureMode::Undefined,
    isDefined(height)?MeasureMode::Exactly:MeasureMode::Undefined,
    width,height,true);
  const auto& margin=layout.margin;
  const auto positionLeft=resolveValue(resolveEdgeRaw(style.position,0),
    isDefined(width)?width:0);
  const auto positionTop=resolveValue(resolveEdgeRaw(style.position,1),
    isDefined(width)?width:0);
  layout.left=margin[0]+(isDefined(positionLeft)?positionLeft:0);
  layout.top=margin[1]+(isDefined(positionTop)?positionTop:0);
  roundLayout(*this,config->pointScaleFactor,0,0);
}
} // namespace batbox::indigenous_yoga
