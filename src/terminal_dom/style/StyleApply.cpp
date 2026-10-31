#include "terminal_dom/style/StyleInternal.hpp"

namespace batbox::terminal_dom::style {
namespace detail {
namespace {
bool isFalse(const Json* value){
  return value&&value->is_boolean()&&!value->get<bool>();
}
void borderSide(FullLayoutNode& node,const char* edge,const char* property,
    const Json& resolved,double borderWidth){
  node.setBorder(edge,isFalse(field(resolved,property))?0:borderWidth);
}
void changedBorderSide(FullLayoutNode& node,const Styles& style,
    const char* property,const char* edge){
  const auto* value=field(style,property);
  if(value&&!undefined(*value))
    node.setBorder(edge,isFalse(value)?0:1);
}
}
void applyBorderStyles(FullLayoutNode& node,const Styles& style,
    const Styles* resolvedStyle){
  const auto& resolved=resolvedStyle&&!nullish(*resolvedStyle)?
    *resolvedStyle:style;
  if(const auto* border=field(style,"borderStyle")){
    const double width=truthy(*border)?1:0;
    borderSide(node,"top","borderTop",resolved,width);
    borderSide(node,"bottom","borderBottom",resolved,width);
    borderSide(node,"left","borderLeft",resolved,width);
    borderSide(node,"right","borderRight",resolved,width);
  }else{
    changedBorderSide(node,style,"borderTop","top");
    changedBorderSide(node,style,"borderBottom","bottom");
    changedBorderSide(node,style,"borderLeft","left");
    changedBorderSide(node,style,"borderRight","right");
  }
}
void applyGapStyles(FullLayoutNode& node,const Styles& style){
  if(const auto* value=field(style,"gap"))
    node.setGap("all",numberOr(value,0));
  if(const auto* value=field(style,"columnGap"))
    node.setGap("column",numberOr(value,0));
  if(const auto* value=field(style,"rowGap"))
    node.setGap("row",numberOr(value,0));
}
} // namespace detail
void applyStyles(FullLayoutNode& node,const Styles& style,
    const Styles* resolvedStyle){
  const std::optional<Styles> defaultStyle=detail::undefined(style)?
    std::optional<Styles>(Styles::object()):std::nullopt;
  const Styles& input=defaultStyle?*defaultStyle:style;
  if(!input.is_object()&&!input.is_array())
    throw TypeError("style is not an Object. (evaluating '\"position\" in style')");
  detail::applyPositionStyles(node,input);
  detail::applyOverflowStyles(node,input);
  detail::applyMarginStyles(node,input);
  detail::applyPaddingStyles(node,input);
  detail::applyFlexStyles(node,input);
  detail::applyDimensionStyles(node,input);
  detail::applyDisplayStyles(node,input);
  detail::applyBorderStyles(node,input,resolvedStyle);
  detail::applyGapStyles(node,input);
}
} // namespace batbox::terminal_dom::style
