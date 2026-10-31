#include "terminal_runtime_roster/MeasureElement.hpp"
#include "terminal_dom/FullLayoutPort.hpp"
#include <iostream>
#include <memory>

using namespace batbox;
class Layout final:public terminal_dom::FullLayoutNode {
public:
  double width=0,height=0;
  void insertChild(terminal_dom::LayoutNode&,std::size_t)override{}
  void removeChild(terminal_dom::LayoutNode&)override{}
  std::size_t getChildCount()const override{return 0;}
  void setMeasureFunc(terminal_dom::MeasureFunction)override{}
  void markDirty()override{}
  terminal_dom::LayoutDisplay getDisplay()const override{return terminal_dom::LayoutDisplay::Flex;}
  double getComputedTop()const override{return 0;}
  double getComputedHeight()const override{return height;}
  terminal_dom::FullLayoutRef getParent()const override{return {};}
  void calculateLayout(std::optional<double>,std::optional<double>)override{}
  void unsetMeasureFunc()override{}
  double getComputedLeft()const override{return 0;}
  double getComputedWidth()const override{return width;}
  double getComputedBorder(const std::string&)const override{return 0;}
  double getComputedPadding(const std::string&)const override{return 0;}
  void setWidth(double)override{}
  void setWidthPercent(double)override{}
  void setWidthAuto()override{}
  void setHeight(double)override{}
  void setHeightPercent(double)override{}
  void setHeightAuto()override{}
  void setMinWidth(double)override{}
  void setMinWidthPercent(double)override{}
  void setMinHeight(double)override{}
  void setMinHeightPercent(double)override{}
  void setMaxWidth(double)override{}
  void setMaxWidthPercent(double)override{}
  void setMaxHeight(double)override{}
  void setMaxHeightPercent(double)override{}
  void setFlexDirection(const std::string&)override{}
  void setFlexGrow(double)override{}
  void setFlexShrink(double)override{}
  void setFlexBasis(double)override{}
  void setFlexBasisPercent(double)override{}
  void setFlexWrap(const std::string&)override{}
  void setAlignItems(const std::string&)override{}
  void setAlignSelf(const std::string&)override{}
  void setJustifyContent(const std::string&)override{}
  void setDisplay(terminal_dom::LayoutDisplay)override{}
  void setPositionType(const std::string&)override{}
  void setPosition(const std::string&,double)override{}
  void setPositionPercent(const std::string&,double)override{}
  void setOverflow(const std::string&)override{}
  void setMargin(const std::string&,double)override{}
  void setPadding(const std::string&,double)override{}
  void setBorder(const std::string&,double)override{}
  void setGap(double){}
  void setGap(const std::string&,double)override{}
  void free()override{}
  void freeRecursive()override{}
};
int main(){
  terminal_dom::DOMElement node("ink-box");
  std::cout<<"[";
  const auto emit=[&](bool comma){
    if(comma)std::cout<<",";
    const auto v=terminal_runtime_roster::measureElement(node);
    std::cout<<"{\"width\":"<<v.width<<",\"height\":"<<v.height<<"}";
  };
  emit(false);
  const auto layout=std::make_shared<Layout>();node.yogaNode=layout;
  layout->width=19;layout->height=7;emit(true);
  layout->width=2.5;layout->height=0;emit(true);
  layout->width=0;layout->height=-3;emit(true);
  std::cout<<"]\n";
}
