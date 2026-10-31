#include "terminal_render/RenderNodeInternal.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"
#include <cmath>
#include <unordered_set>

namespace batbox::terminal_render {
namespace {
std::string spaces(double count){
  if(!std::isfinite(count)||count<0||std::trunc(count)!=count)
    throw std::range_error("Invalid count value");
  return std::string(static_cast<std::size_t>(count),' ');
}
}
void RenderNodeRenderer::renderScrollFast(terminal_dom::DOMElement& content,
    terminal_output::Output& output,double x,double y,double width,
    double contentX,double contentY,double scrollTop,double innerHeight,
    double padTop,std::optional<double> y1,
    const terminal_screen::ScreenRef& prevScreen,bool hasRemovedChild,
    const std::optional<std::string>& boxBackgroundColor,
    const ScrollHint& hint){
  using terminal_screen::typed_array::jsMax;
  using terminal_screen::typed_array::jsMin;
  const auto top=hint.top,bottom=hint.bottom,delta=hint.delta;
  const auto w=std::floor(width);
  output.blit(prevScreen,std::floor(x),top,w,bottom-top+1);
  output.shift(top,bottom,delta);
  const auto edgeTop=delta>0?bottom-delta+1:top;
  const auto edgeBottom=delta>0?bottom:top-delta-1;
  output.clear(detail::rect(std::floor(x),edgeTop,w,
    edgeBottom-edgeTop+1));
  output.clip(std::make_shared<terminal_output::Clip>(
    terminal_output::Clip{{},{},edgeTop,edgeBottom+1}));
  std::optional<std::unordered_set<terminal_dom::DOMNode*>> dirtyChildren;
  if(content.dirty){
    dirtyChildren.emplace();
    for(const auto& child:content.childNodes){
      const auto element=detail::element(child);
      if(element&&element->dirty)dirtyChildren->insert(child.get());
    }
  }
  renderScrolledChildren(content,output,contentX,contentY,
    hasRemovedChild,{},edgeTop-contentY,edgeBottom+1-contentY,
    boxBackgroundColor,true);
  output.unclip();
  if(dirtyChildren){
    const auto edgeTopLocal=edgeTop-contentY;
    const auto edgeBottomLocal=edgeBottom+1-contentY;
    const auto fillSpaces=spaces(w);
    double cumHeightShift=0;
    for(const auto& childNode:content.childNodes){
      const auto childElem=detail::element(childNode);
      if(!childElem)continue;
      const bool isDirty=dirtyChildren->contains(childNode.get());
      if(!isDirty&&cumHeightShift==0){
        if(state_.nodeStash.get(*childElem))continue;
      }
      const auto cy=childElem->yogaNode;
      if(!cy)continue;
      const auto childTop=cy->getComputedTop();
      const auto childH=cy->getComputedHeight();
      const auto childBottom=childTop+childH;
      if(isDirty){
        const auto previous=state_.nodeStash.get(*childElem);
        cumHeightShift+=childH-(previous?previous->height:0);
      }
      if(childBottom<=scrollTop||childTop>=scrollTop+innerHeight)
        continue;
      if(childTop>=edgeTopLocal&&childBottom<=edgeBottomLocal)
        continue;
      const auto screenY=std::floor(contentY+childTop);
      if(!isDirty){
        const auto childStashed=state_.nodeStash.get(*childElem);
        if(childStashed&&std::floor(childStashed->y)-delta==screenY)
          continue;
      }
      const auto screenBottom=jsMin(std::floor(contentY+childBottom),
        std::floor(y1.value_or(y)+padTop+innerHeight));
      if(screenY<screenBottom){
        const auto fill=detail::repeatJoined(fillSpaces,
          screenBottom-screenY);
        output.write(std::floor(x),screenY,fill);
        output.clip(std::make_shared<terminal_output::Clip>(
          terminal_output::Clip{{},{},screenY,screenBottom}));
        render(*childElem,output,RenderNodeOptions{
          contentX,contentY,{},false,boxBackgroundColor});
        output.unclip();
      }
    }
  }
  const auto repairSpaces=state_.absoluteRectsPrev.empty()?std::string{}:
    spaces(w);
  for(const auto& rect:state_.absoluteRectsPrev){
    if(rect->y>=bottom+1||rect->y+rect->height<=top)continue;
    const auto shiftedTop=jsMax(top,std::floor(rect->y)-delta);
    const auto shiftedBottom=jsMin(bottom+1,
      std::floor(rect->y+rect->height)-delta);
    if(shiftedTop>=edgeTop&&shiftedBottom<=edgeBottom+1)continue;
    if(shiftedTop>=shiftedBottom)continue;
    const auto fill=detail::repeatJoined(repairSpaces,
      shiftedBottom-shiftedTop);
    output.write(std::floor(x),shiftedTop,fill);
    output.clip(std::make_shared<terminal_output::Clip>(
      terminal_output::Clip{{},{},shiftedTop,shiftedBottom}));
    renderScrolledChildren(content,output,contentX,contentY,
      hasRemovedChild,{},shiftedTop-contentY,shiftedBottom-contentY,
      boxBackgroundColor,true);
    output.unclip();
  }
}
} // namespace batbox::terminal_render
