#include "terminal_output/Output.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"

namespace batbox::terminal_output {
std::optional<double> maxDefined(std::optional<double> a,
    std::optional<double> b){
  if(!a)return b;
  if(!b)return a;
  return terminal_screen::typed_array::jsMax(*a,*b);
}
std::optional<double> minDefined(std::optional<double> a,
    std::optional<double> b){
  if(!a)return b;
  if(!b)return a;
  return terminal_screen::typed_array::jsMin(*a,*b);
}
ClipRef intersectClip(const ClipRef& parent,const ClipRef& child){
  if(!parent)return child;
  return std::make_shared<Clip>(Clip{
    maxDefined(parent->x1,child->x1),
    minDefined(parent->x2,child->x2),
    maxDefined(parent->y1,child->y1),
    minDefined(parent->y2,child->y2)});
}
} // namespace batbox::terminal_output
