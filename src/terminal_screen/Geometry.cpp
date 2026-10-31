#include "terminal_screen/Geometry.hpp"
#include <cmath>
#include <limits>

namespace batbox::terminal_screen {
namespace {
double jsMin(double a,double b){
  if(std::isnan(a)||std::isnan(b))return
    std::numeric_limits<double>::quiet_NaN();
  if(a==0&&b==0)return std::signbit(a)||std::signbit(b)?-0.0:0.0;
  return a<b?a:b;
}
double jsMax(double a,double b){
  if(std::isnan(a)||std::isnan(b))return
    std::numeric_limits<double>::quiet_NaN();
  if(a==0&&b==0)return std::signbit(a)&&std::signbit(b)?-0.0:0.0;
  return a>b?a:b;
}
double asNumber(std::optional<double> value){
  return value.value_or(std::numeric_limits<double>::quiet_NaN());
}
}
Edges edges(double a,std::optional<double> b,
    std::optional<double> c,std::optional<double> d){
  if(!b)return {a,a,a,a};
  if(!c)return {a,*b,a,*b};
  return {a,*b,*c,d};
}
Edges addEdges(const Edges& a,const Edges& b){
  return {a.top+b.top,a.right+b.right,a.bottom+b.bottom,
    asNumber(a.left)+asNumber(b.left)};
}
const Edges ZERO_EDGES{0,0,0,0};
Edges resolveEdges(const PartialEdges& partial){
  return {partial.top.value_or(0),partial.right.value_or(0),
    partial.bottom.value_or(0),partial.left.value_or(0)};
}
Rectangle unionRect(const Rectangle& a,const Rectangle& b){
  const double minX=jsMin(a.x,b.x);
  const double minY=jsMin(a.y,b.y);
  const double maxX=jsMax(a.x+a.width,b.x+b.width);
  const double maxY=jsMax(a.y+a.height,b.y+b.height);
  return {minX,minY,maxX-minX,maxY-minY};
}
Rectangle clampRect(const Rectangle& rect,const Size& size){
  const double minX=jsMax(0,rect.x);
  const double minY=jsMax(0,rect.y);
  const double maxX=jsMin(size.width-1,rect.x+rect.width-1);
  const double maxY=jsMin(size.height-1,rect.y+rect.height-1);
  return {minX,minY,jsMax(0,maxX-minX+1),
    jsMax(0,maxY-minY+1)};
}
bool withinBounds(const Size& size,const Point& point){
  return point.x>=0&&point.y>=0&&point.x<size.width&&point.y<size.height;
}
double clamp(double value,std::optional<double> min,
    std::optional<double> max){
  if(min&&value<*min)return *min;
  if(max&&value>*max)return *max;
  return value;
}
} // namespace batbox::terminal_screen
