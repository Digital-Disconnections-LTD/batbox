#pragma once
#include "terminal_dom/LayoutPort.hpp"
#include <string>

namespace batbox::terminal_dom {
class FullLayoutNode;
using FullLayoutRef=std::shared_ptr<FullLayoutNode>;
class FullLayoutNode:public LayoutNode {
public:
  virtual FullLayoutRef getParent()const=0;
  virtual void calculateLayout(std::optional<double> width={},
    std::optional<double> height={})=0;
  virtual void unsetMeasureFunc()=0;
  virtual double getComputedLeft()const=0;
  virtual double getComputedWidth()const=0;
  virtual double getComputedBorder(const std::string& edge)const=0;
  virtual double getComputedPadding(const std::string& edge)const=0;
  virtual void setWidth(double)=0;
  virtual void setWidthPercent(double)=0;
  virtual void setWidthAuto()=0;
  virtual void setHeight(double)=0;
  virtual void setHeightPercent(double)=0;
  virtual void setHeightAuto()=0;
  virtual void setMinWidth(double)=0;
  virtual void setMinWidthPercent(double)=0;
  virtual void setMinHeight(double)=0;
  virtual void setMinHeightPercent(double)=0;
  virtual void setMaxWidth(double)=0;
  virtual void setMaxWidthPercent(double)=0;
  virtual void setMaxHeight(double)=0;
  virtual void setMaxHeightPercent(double)=0;
  virtual void setFlexDirection(const std::string&)=0;
  virtual void setFlexGrow(double)=0;
  virtual void setFlexShrink(double)=0;
  virtual void setFlexBasis(double)=0;
  virtual void setFlexBasisPercent(double)=0;
  virtual void setFlexWrap(const std::string&)=0;
  virtual void setAlignItems(const std::string&)=0;
  virtual void setAlignSelf(const std::string&)=0;
  virtual void setJustifyContent(const std::string&)=0;
  virtual void setDisplay(LayoutDisplay)=0;
  virtual void setPositionType(const std::string&)=0;
  virtual void setPosition(const std::string&,double)=0;
  virtual void setPositionPercent(const std::string&,double)=0;
  virtual void setOverflow(const std::string&)=0;
  virtual void setMargin(const std::string&,double)=0;
  virtual void setPadding(const std::string&,double)=0;
  virtual void setBorder(const std::string&,double)=0;
  virtual void setGap(const std::string&,double)=0;
  virtual void free()=0;
  virtual void freeRecursive()=0;
};
} // namespace batbox::terminal_dom
