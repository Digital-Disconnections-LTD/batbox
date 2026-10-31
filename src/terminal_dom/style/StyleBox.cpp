#include "terminal_dom/style/StyleInternal.hpp"

namespace batbox::terminal_dom::style::detail {
namespace {
void applyPositionEdge(FullLayoutNode& node,const char* edge,
    const Json& value){
  if(string(value))node.setPositionPercent(edge,parseInt10(asString(value)));
  else if(number(value))node.setPosition(edge,asNumber(value));
  else node.setPosition(edge,NaN);
}
const Json* coalesce(const Json* first,const Json* fallback){
  return first&&!nullish(*first)?first:fallback;
}
bool named(const Json* value,const char* name){
  return value&&string(*value)&&asString(*value)==name;
}
double orZero(const Json* value){
  return value&&truthy(*value)?asNumber(*value):0;
}
}
void applyPositionStyles(FullLayoutNode& node,const Styles& style){
  if(const auto* value=field(style,"position"))
    node.setPositionType(named(value,"absolute")?"absolute":"relative");
  for(const char* edge:{"top","bottom","left","right"})
    if(const auto* value=field(style,edge))applyPositionEdge(node,edge,*value);
}
void applyOverflowStyles(FullLayoutNode& node,const Styles& style){
  const auto* base=field(style,"overflow");
  const auto* y=coalesce(field(style,"overflowY"),base);
  const auto* x=coalesce(field(style,"overflowX"),base);
  if(named(y,"scroll")||named(x,"scroll"))node.setOverflow("scroll");
  else if(named(y,"hidden")||named(x,"hidden"))node.setOverflow("hidden");
  else if(base||field(style,"overflowX")||field(style,"overflowY"))
    node.setOverflow("visible");
}
void applyMarginStyles(FullLayoutNode& node,const Styles& style){
  if(const auto* value=field(style,"margin"))
    node.setMargin("all",numberOr(value,0));
  if(const auto* value=field(style,"marginX"))
    node.setMargin("horizontal",numberOr(value,0));
  if(const auto* value=field(style,"marginY"))
    node.setMargin("vertical",numberOr(value,0));
  if(const auto* value=field(style,"marginLeft"))
    node.setMargin("start",orZero(value));
  if(const auto* value=field(style,"marginRight"))
    node.setMargin("end",orZero(value));
  if(const auto* value=field(style,"marginTop"))
    node.setMargin("top",orZero(value));
  if(const auto* value=field(style,"marginBottom"))
    node.setMargin("bottom",orZero(value));
}
void applyPaddingStyles(FullLayoutNode& node,const Styles& style){
  if(const auto* value=field(style,"padding"))
    node.setPadding("all",numberOr(value,0));
  if(const auto* value=field(style,"paddingX"))
    node.setPadding("horizontal",numberOr(value,0));
  if(const auto* value=field(style,"paddingY"))
    node.setPadding("vertical",numberOr(value,0));
  if(const auto* value=field(style,"paddingLeft"))
    node.setPadding("left",orZero(value));
  if(const auto* value=field(style,"paddingRight"))
    node.setPadding("right",orZero(value));
  if(const auto* value=field(style,"paddingTop"))
    node.setPadding("top",orZero(value));
  if(const auto* value=field(style,"paddingBottom"))
    node.setPadding("bottom",orZero(value));
}
} // namespace batbox::terminal_dom::style::detail
