#include "terminal_render/RenderNodeInternal.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::terminal_render {
void RenderNodeRenderer::renderBox(terminal_dom::DOMElement& node,
    terminal_output::Output& output,
    const terminal_dom::FullLayoutNode& yoga,double x,double y,
    double width,double height,bool hasRemovedChild,
    const terminal_screen::ScreenRef& prevScreen,bool positionChanged,
    const terminal_dom::StashedRef& stashed,
    const std::optional<std::string>& inheritedBackgroundColor){
  auto boxBackgroundColor=detail::string(node,"backgroundColor");
  if(!boxBackgroundColor)boxBackgroundColor=inheritedBackgroundColor;
  const auto noSelect=detail::field(node,"noSelect");
  if(detail::truthy(noSelect)){
    const auto boxX=std::floor(x);
    const bool fromEdge=noSelect&&noSelect->is_string()&&
      noSelect->get<std::string>()=="from-left-edge";
    output.noSelect(detail::rect(fromEdge?0:boxX,std::floor(y),
      fromEdge?boxX+std::floor(width):std::floor(width),
      std::floor(height)));
  }
  const auto overflowX=detail::string(detail::nullish(
    node,"overflowX","overflow"));
  const auto overflowY=detail::string(detail::nullish(
    node,"overflowY","overflow"));
  const bool clipHorizontally=overflowX=="hidden"||overflowX=="scroll";
  const bool clipVertically=overflowY=="hidden"||overflowY=="scroll";
  const bool isScrollY=overflowY=="scroll";
  const bool needsClip=clipHorizontally||clipVertically;
  std::optional<double> y1,y2;
  if(needsClip){
    const auto x1=clipHorizontally?
      std::optional<double>{x+yoga.getComputedBorder("left")}:
      std::optional<double>{};
    const auto x2=clipHorizontally?
      std::optional<double>{x+yoga.getComputedWidth()-
        yoga.getComputedBorder("right")}:
      std::optional<double>{};
    y1=clipVertically?
      std::optional<double>{y+yoga.getComputedBorder("top")}:
      std::optional<double>{};
    y2=clipVertically?
      std::optional<double>{y+yoga.getComputedHeight()-
        yoga.getComputedBorder("bottom")}:
      std::optional<double>{};
    output.clip(std::make_shared<terminal_output::Clip>(
      terminal_output::Clip{x1,x2,y1,y2}));
  }
  if(isScrollY){
    renderScrollBox(node,output,yoga,x,y,width,height,y1,y2,
      hasRemovedChild,prevScreen,positionChanged,stashed,boxBackgroundColor);
  }else{
    const auto ownBackgroundColor=detail::string(node,"backgroundColor");
    const bool opaque=detail::truthy(node,"opaque");
    if((ownBackgroundColor&&!ownBackgroundColor->empty())||opaque){
      const auto borderLeft=yoga.getComputedBorder("left");
      const auto borderRight=yoga.getComputedBorder("right");
      const auto borderTop=yoga.getComputedBorder("top");
      const auto borderBottom=yoga.getComputedBorder("bottom");
      const auto innerWidth=std::floor(width)-borderLeft-borderRight;
      const auto innerHeight=std::floor(height)-borderTop-borderBottom;
      if(innerWidth>0&&innerHeight>0){
        if(!std::isfinite(innerWidth)||std::trunc(innerWidth)!=innerWidth)
          throw std::range_error("Invalid count value");
        const auto spaces=std::string(static_cast<std::size_t>(innerWidth),' ');
        auto fillLine=spaces;
        if(ownBackgroundColor&&!ownBackgroundColor->empty()){
          if(!imports_.text.applyTextStyles)
            throw std::invalid_argument("Text style import required");
          auto styles=std::make_shared<Json>(
            Json{{"backgroundColor",*ownBackgroundColor}});
          fillLine=imports_.text.applyTextStyles(spaces,styles);
        }
        const auto fill=detail::repeatJoined(fillLine,innerHeight);
        output.write(x+borderLeft,y+borderTop,fill);
      }
    }
    renderChildren(node,output,x,y,hasRemovedChild,
      ((ownBackgroundColor&&!ownBackgroundColor->empty())||opaque)?
        terminal_screen::ScreenRef{}:prevScreen,boxBackgroundColor);
  }
  if(needsClip)output.unclip();
  renderBorder(x,y,node,output,imports_.border);
}
} // namespace batbox::terminal_render
