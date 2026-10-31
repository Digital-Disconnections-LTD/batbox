#pragma once
#include "indigenous_yoga/Enums.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace batbox::indigenous_yoga {
struct TypeError:std::runtime_error {using std::runtime_error::runtime_error;};
inline constexpr double NaN=std::numeric_limits<double>::quiet_NaN();
inline double jsMax(double a,double b){
  if(std::isnan(a)||std::isnan(b))return NaN;
  if(a==0&&b==0)return std::signbit(a)&&std::signbit(b)?-0.0:0.0;
  return std::max(a,b);
}
inline double jsMin(double a,double b){
  if(std::isnan(a)||std::isnan(b))return NaN;
  if(a==0&&b==0)return std::signbit(a)||std::signbit(b)?-0.0:0.0;
  return std::min(a,b);
}
struct Value {Unit unit=Unit::Undefined;double value=NaN;};
inline constexpr Value UNDEFINED_VALUE{Unit::Undefined,NaN};
inline constexpr Value AUTO_VALUE{Unit::Auto,NaN};
Value pointValue(double value);
Value percentValue(double value);
double resolveValue(Value value,double ownerSize);
bool isDefined(double value);
bool sameFloat(double a,double b);

using Edges=std::array<double,4>;
using StyleEdges=std::array<Value,9>;
using Gaps=std::array<Value,3>;
struct Layout {
  double left=0,top=0,width=0,height=0;
  Edges border{},padding{},margin{};
};
struct Style {
  Direction direction=Direction::Inherit;
  FlexDirection flexDirection=FlexDirection::Column;
  Justify justifyContent=Justify::FlexStart;
  Align alignItems=Align::Stretch,alignSelf=Align::Auto,
    alignContent=Align::FlexStart;
  Wrap flexWrap=Wrap::NoWrap;
  Overflow overflow=Overflow::Visible;
  Display display=Display::Flex;
  PositionType positionType=PositionType::Relative;
  double flexGrow=0,flexShrink=0;
  Value flexBasis=AUTO_VALUE;
  StyleEdges margin{},padding{},border{},position{};
  Gaps gap{};
  Value width=AUTO_VALUE,height=AUTO_VALUE;
  Value minWidth=UNDEFINED_VALUE,minHeight=UNDEFINED_VALUE,
    maxWidth=UNDEFINED_VALUE,maxHeight=UNDEFINED_VALUE;
};
Style defaultStyle();
double resolveEdge(const StyleEdges& edges,int physicalEdge,
  double ownerSize,bool allowAuto=false);
Value resolveEdgeRaw(const StyleEdges& edges,int physicalEdge);
bool isMarginAuto(const StyleEdges& edges,int physicalEdge);
bool hasAnyAutoEdge(const StyleEdges& edges);
bool hasAnyDefinedEdge(const StyleEdges& edges);
void resolveEdges4Into(const StyleEdges& edges,double ownerSize,Edges& out);
bool isRow(FlexDirection dir);
bool isReverse(FlexDirection dir);
FlexDirection crossAxis(FlexDirection dir);
int leadingEdge(FlexDirection dir);
int trailingEdge(FlexDirection dir);
struct Size {std::optional<double> width,height;};
using MeasureFunction=std::function<Size(double,MeasureMode,double,MeasureMode)>;

struct Config {
  double pointScaleFactor=1;
  Errata errata=Errata::None;
  bool useWebDefaults=false;
  void free(){}
  bool isExperimentalFeatureEnabled(ExperimentalFeature)const{return false;}
  void setExperimentalFeatureEnabled(ExperimentalFeature,bool){}
  void setPointScaleFactor(double value){pointScaleFactor=value;}
  Errata getErrata()const{return errata;}
  void setErrata(Errata value){errata=value;}
  void setUseWebDefaults(bool value){useWebDefaults=value;}
};
using ConfigRef=std::shared_ptr<Config>;
ConfigRef createConfig();
ConfigRef defaultConfig();
Value parseDimension(const std::variant<double,std::string,std::monostate>& value);
int physicalEdge(Edge edge);
} // namespace batbox::indigenous_yoga
