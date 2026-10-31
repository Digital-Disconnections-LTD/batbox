#pragma once
#include <optional>

namespace batbox::terminal_screen {
struct Point {double x=0,y=0;};
struct Size {double width=0,height=0;};
struct Rectangle {double x=0,y=0,width=0,height=0;};
struct Edges {
  double top=0,right=0,bottom=0;
  std::optional<double> left=0;
};
struct PartialEdges {
  std::optional<double> top,right,bottom,left;
};
Edges edges(double a,std::optional<double> b={},
  std::optional<double> c={},std::optional<double> d={});
Edges addEdges(const Edges& a,const Edges& b);
extern const Edges ZERO_EDGES;
Edges resolveEdges(const PartialEdges& partial={});
Rectangle unionRect(const Rectangle& a,const Rectangle& b);
Rectangle clampRect(const Rectangle& rect,const Size& size);
bool withinBounds(const Size& size,const Point& point);
double clamp(double value,std::optional<double> min={},
  std::optional<double> max={});
} // namespace batbox::terminal_screen
