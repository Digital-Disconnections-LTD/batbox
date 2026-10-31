#include "terminal_render/RenderNodeInternal.hpp"
#include <cmath>

namespace batbox::terminal_render {
void RenderNodeRenderer::renderChildren(terminal_dom::DOMElement& node,
    terminal_output::Output& output,double offsetX,double offsetY,
    bool hasRemovedChild,const terminal_screen::ScreenRef& prevScreen,
    const std::optional<std::string>& inheritedBackgroundColor){
  bool seenDirtyChild=false,seenDirtyClipped=false;
  for(const auto& childNode:node.childNodes){
    const auto childElem=detail::element(childNode);
    if(!childElem)continue;
    const bool wasDirty=childElem->dirty;
    const bool isAbsolute=detail::string(*childElem,"position")=="absolute";
    render(*childElem,output,RenderNodeOptions{
      offsetX,offsetY,hasRemovedChild||seenDirtyChild?
        terminal_screen::ScreenRef{}:prevScreen,
      seenDirtyClipped&&isAbsolute&&
        !detail::truthy(*childElem,"opaque")&&
        detail::field(*childElem,"backgroundColor")==nullptr,
      inheritedBackgroundColor});
    if(wasDirty&&!seenDirtyChild){
      if(!clipsBothAxes(*childElem)||isAbsolute)seenDirtyChild=true;
      else seenDirtyClipped=true;
    }
  }
}
bool RenderNodeRenderer::clipsBothAxes(
    const terminal_dom::DOMElement& node)const{
  const auto ox=detail::string(detail::nullish(node,"overflowX","overflow"));
  const auto oy=detail::string(detail::nullish(node,"overflowY","overflow"));
  return (ox=="hidden"||ox=="scroll")&&
    (oy=="hidden"||oy=="scroll");
}
bool RenderNodeRenderer::siblingSharesY(const terminal_dom::DOMElement& node,
    const terminal_dom::FullLayoutNode& yogaNode)const{
  if(!node.parentNode)return false;
  const auto parent=static_cast<terminal_dom::DOMElement*>(
    static_cast<terminal_dom::DOMNode*>(node.parentNode));
  const auto myTop=yogaNode.getComputedTop();
  const auto& siblings=parent->childNodes;
  auto index=siblings.size();
  for(std::size_t i=0;i<siblings.size();i++)
    if(siblings[i].get()==&node){index=i;break;}
  for(auto i=index+1;i<siblings.size();i++){
    const auto sib=siblings[i]?siblings[i]->yogaNode:
      terminal_dom::LayoutRef{};
    if(!sib)continue;
    return sib->getComputedTop()==myTop;
  }
  for(auto i=index;i>0;){
    --i;
    const auto sib=siblings[i]?siblings[i]->yogaNode:
      terminal_dom::LayoutRef{};
    if(!sib)continue;
    return sib->getComputedTop()==myTop;
  }
  return false;
}
void RenderNodeRenderer::blitEscapingAbsoluteDescendants(
    terminal_dom::DOMElement& node,terminal_output::Output& output,
    const terminal_screen::ScreenRef& prevScreen,double px,double py,
    double pw,double ph){
  const auto pr=px+pw,pb=py+ph;
  for(const auto& child:node.childNodes){
    if(!child||child->nodeName=="#text")continue;
    const auto elem=detail::element(child);
    if(!elem)continue;
    if(detail::string(*elem,"position")=="absolute"){
      const auto stashed=state_.nodeStash.get(*elem);
      if(stashed){
        state_.absoluteRectsCur.push_back(stashed);
        const auto cx=std::floor(stashed->x),cy=std::floor(stashed->y);
        const auto cw=std::floor(stashed->width),
          ch=std::floor(stashed->height);
        if(cx<px||cy<py||cx+cw>pr||cy+ch>pb)
          output.blit(prevScreen,cx,cy,cw,ch);
      }
    }
    blitEscapingAbsoluteDescendants(*elem,output,prevScreen,px,py,pw,ph);
  }
}
void RenderNodeRenderer::dropSubtreeStash(terminal_dom::DOMElement& node){
  state_.nodeStash.erase(node);
  for(const auto& child:node.childNodes){
    if(child&&child->nodeName!="#text"){
      const auto elem=detail::element(child);
      if(elem)dropSubtreeStash(*elem);
    }
  }
}
void RenderNodeRenderer::renderScrolledChildren(terminal_dom::DOMElement& node,
    terminal_output::Output& output,double offsetX,double offsetY,
    bool hasRemovedChild,const terminal_screen::ScreenRef& prevScreen,
    double scrollTopY,double scrollBottomY,
    const std::optional<std::string>& inheritedBackgroundColor,
    bool preserveCulledStash){
  bool seenDirtyChild=false;
  double cumHeightShift=0;
  for(const auto& childNode:node.childNodes){
    const auto childElem=detail::element(childNode);
    if(!childElem)continue;
    const auto cy=childElem->yogaNode;
    if(cy){
      const auto stashed=state_.nodeStash.get(*childElem);
      double top,height;
      if(stashed&&stashed->top&&!childElem->dirty&&cumHeightShift==0){
        top=*stashed->top;height=stashed->height;
      }else{
        top=cy->getComputedTop();height=cy->getComputedHeight();
        if(childElem->dirty)
          cumHeightShift+=height-(stashed?stashed->height:0);
        if(stashed)stashed->top=top;
      }
      const auto bottom=top+height;
      if(bottom<=scrollTopY||top>=scrollBottomY){
        if(!preserveCulledStash)dropSubtreeStash(*childElem);
        continue;
      }
    }
    const bool wasDirty=childElem->dirty;
    render(*childElem,output,RenderNodeOptions{
      offsetX,offsetY,hasRemovedChild||seenDirtyChild?
        terminal_screen::ScreenRef{}:prevScreen,
      false,inheritedBackgroundColor});
    if(wasDirty)seenDirtyChild=true;
  }
}
} // namespace batbox::terminal_render
