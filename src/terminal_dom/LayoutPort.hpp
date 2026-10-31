#pragma once
#include "terminal_screen/Geometry.hpp"
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>

namespace batbox::terminal_dom {
enum class LayoutDisplay {Flex,None};
enum class LayoutMeasureMode {Undefined,Exactly,AtMost};
struct MeasureSize {
  std::optional<double> width,height;
};
using MeasureFunction=std::function<MeasureSize(double,LayoutMeasureMode)>;
// The Yoga adapter supplies this interface. DOM operations require exactly
// these methods and never simulate a layout calculation.
class LayoutNode {
public:
  virtual ~LayoutNode()=default;
  virtual void insertChild(LayoutNode& child,std::size_t index)=0;
  virtual void removeChild(LayoutNode& child)=0;
  virtual std::size_t getChildCount()const=0;
  virtual void setMeasureFunc(MeasureFunction callback)=0;
  virtual void markDirty()=0;
  virtual LayoutDisplay getDisplay()const=0;
  virtual double getComputedTop()const=0;
  virtual double getComputedHeight()const=0;
};
using LayoutRef=std::shared_ptr<LayoutNode>;
using LayoutFactory=std::function<LayoutRef()>;
} // namespace batbox::terminal_dom
