#include "terminal_render/RenderNodeInternal.hpp"
#include <cmath>

namespace batbox::terminal_render {
RenderNodeRenderer::RenderNodeRenderer(RenderState& state,
    RenderNodeImports imports):state_(state),imports_(std::move(imports)){}
void RenderNodeRenderer::render(terminal_dom::DOMElement& node,
    terminal_output::Output& output,RenderNodeOptions options){
  const auto yoga=detail::fullLayout(node);
  if(!yoga)return;
  if(yoga->getDisplay()==terminal_dom::LayoutDisplay::None){
    if(node.dirty){
      const auto stashed=state_.nodeStash.get(node);
      if(stashed){
        output.clear(detail::rect(std::floor(stashed->x),
          std::floor(stashed->y),std::floor(stashed->width),
          std::floor(stashed->height)));
        dropSubtreeStash(node);
        state_.layoutShifted=true;
      }
    }
    return;
  }
  const auto x=options.offsetX+yoga->getComputedLeft();
  const auto yogaTop=yoga->getComputedTop();
  double y=options.offsetY+yogaTop;
  const auto width=yoga->getComputedWidth();
  const auto height=yoga->getComputedHeight();
  const bool absolute=detail::string(node,"position")=="absolute";
  if(y<0&&absolute)y=0;
  const auto stashed=state_.nodeStash.get(node);
  if(!node.dirty&&!options.skipSelfBlit&&
      !node.pendingScrollDelta&&stashed&&stashed->x==x&&stashed->y==y&&
      stashed->width==width&&stashed->height==height&&options.prevScreen){
    const auto fx=std::floor(x),fy=std::floor(y);
    const auto fw=std::floor(width),fh=std::floor(height);
    output.blit(options.prevScreen,fx,fy,fw,fh);
    if(absolute)state_.absoluteRectsCur.push_back(stashed);
    blitEscapingAbsoluteDescendants(node,output,options.prevScreen,
      fx,fy,fw,fh);
    return;
  }
  const bool positionChanged=stashed&&
    (stashed->x!=x||stashed->y!=y||stashed->width!=width||
      stashed->height!=height);
  if(positionChanged)state_.layoutShifted=true;
  if(stashed&&(node.dirty||positionChanged))
    output.clear(detail::rect(std::floor(stashed->x),
      std::floor(stashed->y),std::floor(stashed->width),
      std::floor(stashed->height)),absolute);
  const auto clears=state_.nodeStash.pendingClears(node);
  const bool hasRemovedChild=clears!=nullptr;
  if(hasRemovedChild){
    state_.layoutShifted=true;
    for(const auto& rect:*clears)
      output.clear(detail::rect(std::floor(rect->x),
        std::floor(rect->y),std::floor(rect->width),
        std::floor(rect->height)));
    state_.nodeStash.erasePendingClears(node);
  }
  if(height==0&&siblingSharesY(node,*yoga)){
    state_.nodeStash.set(node,std::make_shared<terminal_dom::StashedLayout>(
      terminal_dom::StashedLayout{x,y,width,height,yogaTop}));
    node.dirty=false;
    return;
  }
  if(node.nodeName=="ink-raw-ansi"){
    if(node.attributes.is_object()){
      const auto raw=node.attributes.find("rawText");
      if(raw!=node.attributes.end()&&raw->is_string()&&
          !raw->get<std::string>().empty())
        output.write(x,y,raw->get<std::string>());
    }
  }else if(node.nodeName=="ink-text"){
    renderText(node,output,*yoga,x,y,options.inheritedBackgroundColor);
  }else if(node.nodeName=="ink-box"){
    renderBox(node,output,*yoga,x,y,width,height,hasRemovedChild,
      options.prevScreen,positionChanged,stashed,
      options.inheritedBackgroundColor);
  }else if(node.nodeName=="ink-root"){
    renderChildren(node,output,x,y,hasRemovedChild,options.prevScreen,
      options.inheritedBackgroundColor);
  }
  auto rect=std::make_shared<terminal_dom::StashedLayout>(
    terminal_dom::StashedLayout{x,y,width,height,yogaTop});
  state_.nodeStash.set(node,rect);
  if(absolute)state_.absoluteRectsCur.push_back(rect);
  node.dirty=false;
}
} // namespace batbox::terminal_render
