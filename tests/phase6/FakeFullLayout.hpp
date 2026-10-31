#pragma once
#include "terminal_dom/FullLayoutPort.hpp"

struct FakeFullLayout:batbox::terminal_dom::FullLayoutNode {
  double left=0,top=0,width=0,height=0;
  void insertChild(batbox::terminal_dom::LayoutNode&,std::size_t)override{}
  void removeChild(batbox::terminal_dom::LayoutNode&)override{}
  std::size_t getChildCount()const override{return 0;}
  void setMeasureFunc(batbox::terminal_dom::MeasureFunction)override{}
  void markDirty()override{}
  batbox::terminal_dom::LayoutDisplay getDisplay()const override{
    return batbox::terminal_dom::LayoutDisplay::Flex;}
  double getComputedTop()const override{return top;}
  double getComputedHeight()const override{return height;}
  batbox::terminal_dom::FullLayoutRef getParent()const override{return {};}
  void calculateLayout(std::optional<double>,std::optional<double>)override{}
  void unsetMeasureFunc()override{}
  double getComputedLeft()const override{return left;}
  double getComputedWidth()const override{return width;}
  double getComputedBorder(const std::string&)const override{return 0;}
  double getComputedPadding(const std::string&)const override{return 0;}
#define SET_DOUBLE(name) void name(double)override{}
#define SET_STRING(name) void name(const std::string&)override{}
#define SET_STRING_DOUBLE(name) void name(const std::string&,double)override{}
  SET_DOUBLE(setWidth) SET_DOUBLE(setWidthPercent)
  void setWidthAuto()override{}
  SET_DOUBLE(setHeight) SET_DOUBLE(setHeightPercent)
  void setHeightAuto()override{}
  SET_DOUBLE(setMinWidth) SET_DOUBLE(setMinWidthPercent)
  SET_DOUBLE(setMinHeight) SET_DOUBLE(setMinHeightPercent)
  SET_DOUBLE(setMaxWidth) SET_DOUBLE(setMaxWidthPercent)
  SET_DOUBLE(setMaxHeight) SET_DOUBLE(setMaxHeightPercent)
  SET_STRING(setFlexDirection) SET_DOUBLE(setFlexGrow)
  SET_DOUBLE(setFlexShrink) SET_DOUBLE(setFlexBasis)
  SET_DOUBLE(setFlexBasisPercent) SET_STRING(setFlexWrap)
  SET_STRING(setAlignItems) SET_STRING(setAlignSelf)
  SET_STRING(setJustifyContent)
  void setDisplay(batbox::terminal_dom::LayoutDisplay)override{}
  SET_STRING(setPositionType) SET_STRING_DOUBLE(setPosition)
  SET_STRING_DOUBLE(setPositionPercent) SET_STRING(setOverflow)
  SET_STRING_DOUBLE(setMargin) SET_STRING_DOUBLE(setPadding)
  SET_STRING_DOUBLE(setBorder) SET_STRING_DOUBLE(setGap)
  void free()override{}
  void freeRecursive()override{}
#undef SET_DOUBLE
#undef SET_STRING
#undef SET_STRING_DOUBLE
};
