#pragma once
#include "terminal_dom/FullLayoutPort.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::terminal_dom::style_test {
class TraceLayoutNode:public FullLayoutNode {
public:
  Json trace=Json::array();
  void insertChild(LayoutNode&,std::size_t)override{throw std::logic_error("unused");}
  void removeChild(LayoutNode&)override{throw std::logic_error("unused");}
  std::size_t getChildCount()const override{throw std::logic_error("unused");}
  FullLayoutRef getParent()const override{throw std::logic_error("unused");}
  void calculateLayout(std::optional<double>,std::optional<double>)override{
    throw std::logic_error("unused");}
  void setMeasureFunc(MeasureFunction)override{throw std::logic_error("unused");}
  void unsetMeasureFunc()override{throw std::logic_error("unused");}
  void markDirty()override{throw std::logic_error("unused");}
  double getComputedLeft()const override{throw std::logic_error("unused");}
  double getComputedTop()const override{throw std::logic_error("unused");}
  double getComputedWidth()const override{throw std::logic_error("unused");}
  double getComputedHeight()const override{throw std::logic_error("unused");}
  double getComputedBorder(const std::string&)const override{
    throw std::logic_error("unused");}
  double getComputedPadding(const std::string&)const override{
    throw std::logic_error("unused");}
  LayoutDisplay getDisplay()const override{throw std::logic_error("unused");}
  void free()override{throw std::logic_error("unused");}
  void freeRecursive()override{throw std::logic_error("unused");}
#define TRACE_DOUBLE(name) void name(double value)override{trace.push_back(Json::array({#name,value}));}
#define TRACE_STRING(name) void name(const std::string& value)override{trace.push_back(Json::array({#name,value}));}
#define TRACE_EDGE(name) void name(const std::string& edge,double value)override{trace.push_back(Json::array({#name,edge,value}));}
  TRACE_DOUBLE(setWidth)
  TRACE_DOUBLE(setWidthPercent)
  void setWidthAuto()override{trace.push_back(Json::array({"setWidthAuto"}));}
  TRACE_DOUBLE(setHeight)
  TRACE_DOUBLE(setHeightPercent)
  void setHeightAuto()override{trace.push_back(Json::array({"setHeightAuto"}));}
  TRACE_DOUBLE(setMinWidth)
  TRACE_DOUBLE(setMinWidthPercent)
  TRACE_DOUBLE(setMinHeight)
  TRACE_DOUBLE(setMinHeightPercent)
  TRACE_DOUBLE(setMaxWidth)
  TRACE_DOUBLE(setMaxWidthPercent)
  TRACE_DOUBLE(setMaxHeight)
  TRACE_DOUBLE(setMaxHeightPercent)
  TRACE_STRING(setFlexDirection)
  TRACE_DOUBLE(setFlexGrow)
  TRACE_DOUBLE(setFlexShrink)
  TRACE_DOUBLE(setFlexBasis)
  TRACE_DOUBLE(setFlexBasisPercent)
  TRACE_STRING(setFlexWrap)
  TRACE_STRING(setAlignItems)
  TRACE_STRING(setAlignSelf)
  TRACE_STRING(setJustifyContent)
  void setDisplay(LayoutDisplay value)override{
    trace.push_back(Json::array({"setDisplay",value==LayoutDisplay::Flex?"flex":"none"}));
  }
  TRACE_STRING(setPositionType)
  TRACE_EDGE(setPosition)
  TRACE_EDGE(setPositionPercent)
  TRACE_STRING(setOverflow)
  TRACE_EDGE(setMargin)
  TRACE_EDGE(setPadding)
  TRACE_EDGE(setBorder)
  TRACE_EDGE(setGap)
#undef TRACE_DOUBLE
#undef TRACE_STRING
#undef TRACE_EDGE
};
} // namespace batbox::terminal_dom::style_test
