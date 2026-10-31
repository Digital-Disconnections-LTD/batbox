#include "terminal_dom/YogaMaps.hpp"
#include <string>
#include <stdexcept>

namespace batbox::terminal_dom::yoga_map {
namespace {
[[noreturn]] void invalid(std::string_view type,std::string_view value){
  throw std::invalid_argument("Invalid layout "+std::string(type)+": "+
    std::string(value));
}
}
indigenous_yoga::Edge edge(std::string_view name){
  using E=indigenous_yoga::Edge;
  if(name=="all")return E::All;
  if(name=="horizontal")return E::Horizontal;
  if(name=="vertical")return E::Vertical;
  if(name=="left")return E::Left;
  if(name=="right")return E::Right;
  if(name=="top")return E::Top;
  if(name=="bottom")return E::Bottom;
  if(name=="start")return E::Start;
  if(name=="end")return E::End;
  invalid("edge",name);
}
indigenous_yoga::Gutter gutter(std::string_view name){
  using G=indigenous_yoga::Gutter;
  if(name=="all")return G::All;
  if(name=="column")return G::Column;
  if(name=="row")return G::Row;
  invalid("gutter",name);
}
indigenous_yoga::FlexDirection flexDirection(std::string_view name){
  using F=indigenous_yoga::FlexDirection;
  if(name=="row")return F::Row;
  if(name=="row-reverse")return F::RowReverse;
  if(name=="column")return F::Column;
  if(name=="column-reverse")return F::ColumnReverse;
  invalid("flex direction",name);
}
indigenous_yoga::Align align(std::string_view name){
  using A=indigenous_yoga::Align;
  if(name=="auto")return A::Auto;
  if(name=="stretch")return A::Stretch;
  if(name=="flex-start")return A::FlexStart;
  if(name=="center")return A::Center;
  if(name=="flex-end")return A::FlexEnd;
  invalid("alignment",name);
}
indigenous_yoga::Justify justify(std::string_view name){
  using J=indigenous_yoga::Justify;
  if(name=="flex-start")return J::FlexStart;
  if(name=="center")return J::Center;
  if(name=="flex-end")return J::FlexEnd;
  if(name=="space-between")return J::SpaceBetween;
  if(name=="space-around")return J::SpaceAround;
  if(name=="space-evenly")return J::SpaceEvenly;
  invalid("justification",name);
}
indigenous_yoga::Wrap wrap(std::string_view name){
  using W=indigenous_yoga::Wrap;
  if(name=="nowrap")return W::NoWrap;
  if(name=="wrap")return W::Wrap;
  if(name=="wrap-reverse")return W::WrapReverse;
  invalid("wrap",name);
}
indigenous_yoga::Overflow overflow(std::string_view name){
  using O=indigenous_yoga::Overflow;
  if(name=="visible")return O::Visible;
  if(name=="hidden")return O::Hidden;
  if(name=="scroll")return O::Scroll;
  invalid("overflow",name);
}
indigenous_yoga::PositionType positionType(std::string_view name){
  return name=="absolute"?indigenous_yoga::PositionType::Absolute:
    indigenous_yoga::PositionType::Relative;
}
} // namespace batbox::terminal_dom::yoga_map
