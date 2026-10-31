#pragma once
#include "indigenous_yoga/Node.hpp"
#include "terminal_dom/FullLayoutPort.hpp"

namespace batbox::terminal_dom {
class YogaLayoutNode final:public FullLayoutNode {
public:
  explicit YogaLayoutNode(indigenous_yoga::NodeRef yoga);
  const indigenous_yoga::NodeRef& yoga()const{return yoga_;}
  void insertChild(LayoutNode& child,std::size_t index)override;
  void removeChild(LayoutNode& child)override;
  std::size_t getChildCount()const override;
  FullLayoutRef getParent()const override;
  void calculateLayout(std::optional<double> width={},
    std::optional<double> height={})override;
  void setMeasureFunc(MeasureFunction callback)override;
  void unsetMeasureFunc()override;
  void markDirty()override;
  double getComputedLeft()const override;
  double getComputedTop()const override;
  double getComputedWidth()const override;
  double getComputedHeight()const override;
  double getComputedBorder(const std::string& edge)const override;
  double getComputedPadding(const std::string& edge)const override;
  LayoutDisplay getDisplay()const override;
  void setWidth(double)override;
  void setWidthPercent(double)override;
  void setWidthAuto()override;
  void setHeight(double)override;
  void setHeightPercent(double)override;
  void setHeightAuto()override;
  void setMinWidth(double)override;
  void setMinWidthPercent(double)override;
  void setMinHeight(double)override;
  void setMinHeightPercent(double)override;
  void setMaxWidth(double)override;
  void setMaxWidthPercent(double)override;
  void setMaxHeight(double)override;
  void setMaxHeightPercent(double)override;
  void setFlexDirection(const std::string&)override;
  void setFlexGrow(double)override;
  void setFlexShrink(double)override;
  void setFlexBasis(double)override;
  void setFlexBasisPercent(double)override;
  void setFlexWrap(const std::string&)override;
  void setAlignItems(const std::string&)override;
  void setAlignSelf(const std::string&)override;
  void setJustifyContent(const std::string&)override;
  void setDisplay(LayoutDisplay)override;
  void setPositionType(const std::string&)override;
  void setPosition(const std::string&,double)override;
  void setPositionPercent(const std::string&,double)override;
  void setOverflow(const std::string&)override;
  void setMargin(const std::string&,double)override;
  void setPadding(const std::string&,double)override;
  void setBorder(const std::string&,double)override;
  void setGap(const std::string&,double)override;
  void free()override;
  void freeRecursive()override;
private:
  indigenous_yoga::NodeRef yoga_;
};
FullLayoutRef createYogaLayoutNode();
FullLayoutRef createLayoutNode();
} // namespace batbox::terminal_dom
