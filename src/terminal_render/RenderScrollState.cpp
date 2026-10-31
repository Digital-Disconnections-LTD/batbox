#include "terminal_render/RenderNodeInternal.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"
#include <cmath>

namespace batbox::terminal_render {
void RenderNodeRenderer::renderScrollBox(terminal_dom::DOMElement& node,
    terminal_output::Output& output,
    const terminal_dom::FullLayoutNode& yoga,double x,double y,
    double width,double height,std::optional<double> y1,
    std::optional<double> y2,bool hasRemovedChild,
    const terminal_screen::ScreenRef& prevScreen,bool positionChanged,
    const terminal_dom::StashedRef& stashed,
    const std::optional<std::string>& boxBackgroundColor){
  using terminal_screen::typed_array::jsMax;
  using terminal_screen::typed_array::jsMin;
  const auto padTop=yoga.getComputedPadding("top");
  const auto innerHeight=jsMax(0,(y2.value_or(y+height))-
    (y1.value_or(y))-padTop-yoga.getComputedPadding("bottom"));
  terminal_dom::ElementRef content;
  for(const auto& child:node.childNodes){
    if(child&&child->yogaNode){content=detail::element(child);break;}
  }
  const auto contentYoga=content?detail::fullLayout(*content):
    terminal_dom::FullLayoutRef{};
  const auto scrollHeight=contentYoga?contentYoga->getComputedHeight():0;
  const auto prevScrollHeight=node.scrollHeight.value_or(scrollHeight);
  const auto prevInnerHeight=node.scrollViewportHeight.value_or(innerHeight);
  node.scrollHeight=scrollHeight;
  node.scrollViewportHeight=innerHeight;
  node.scrollViewportTop=y1.value_or(y)+padTop;
  const auto maxScroll=jsMax(0,scrollHeight-innerHeight);
  if(node.scrollAnchor){
    const auto anchor=node.scrollAnchor->el;
    if(anchor&&anchor->yogaNode){
      node.scrollTop=anchor->yogaNode->getComputedTop()+
        node.scrollAnchor->offset;
      node.pendingScrollDelta.reset();
    }
    node.scrollAnchor.reset();
  }
  const auto scrollTopBeforeFollow=node.scrollTop.value_or(0);
  bool sticky=false;
  if(node.stickyScroll)sticky=*node.stickyScroll;
  else if(node.attributes.is_object()){
    const auto value=node.attributes.find("stickyScroll");
    sticky=value!=node.attributes.end()&&value::truthy(*value);
  }
  const auto prevMaxScroll=jsMax(0,prevScrollHeight-prevInnerHeight);
  const bool grew=scrollHeight>=prevScrollHeight;
  const bool atBottom=sticky||
    (grew&&scrollTopBeforeFollow>=prevMaxScroll);
  if(atBottom&&node.pendingScrollDelta.value_or(0)>=0){
    node.scrollTop=maxScroll;
    node.pendingScrollDelta.reset();
    if(node.stickyScroll&&!(*node.stickyScroll)&&
        scrollTopBeforeFollow>=prevMaxScroll)
      node.stickyScroll=true;
  }
  const auto followDelta=node.scrollTop.value_or(0)-scrollTopBeforeFollow;
  if(followDelta>0){
    const auto vpTop=node.scrollViewportTop.value_or(0);
    state_.followScroll=FollowScroll{
      followDelta,vpTop,vpTop+innerHeight-1};
  }
  auto cur=node.scrollTop.value_or(0);
  const auto pending=node.pendingScrollDelta;
  const auto cMin=node.scrollClampMin,cMax=node.scrollClampMax;
  const bool haveClamp=cMin.has_value()&&cMax.has_value();
  if(pending&&*pending!=0){
    const bool pastClamp=haveClamp&&
      ((*pending<0&&cur<*cMin)||(*pending>0&&cur>*cMax));
    const auto eff=pastClamp?
      jsMin(4,static_cast<std::int32_t>(
        terminal_screen::typed_array::toInt32(innerHeight))>>3):innerHeight;
    cur+=isXtermJsHost(imports_.termProgram,imports_.isXtermJs)?
      drainAdaptive(node,*pending,eff):
      drainProportional(node,*pending,eff);
  }else if(pending&&*pending==0){
    node.pendingScrollDelta.reset();
  }
  auto scrollTop=jsMax(0,jsMin(cur,maxScroll));
  const auto clamped=haveClamp?jsMax(*cMin,jsMin(scrollTop,*cMax)):
    scrollTop;
  node.scrollTop=scrollTop;
  if(scrollTop!=cur)node.pendingScrollDelta.reset();
  if(node.pendingScrollDelta)
    state_.scrollDrainNode=std::static_pointer_cast<terminal_dom::DOMElement>(
      node.shared_from_this());
  scrollTop=clamped;
  if(content&&contentYoga){
    const auto contentX=x+contentYoga->getComputedLeft();
    const auto contentY=y+contentYoga->getComputedTop()-scrollTop;
    const auto contentStashed=state_.nodeStash.get(*content);
    std::optional<ScrollHint> hint;
    if(contentStashed&&contentStashed->y!=contentY){
      const auto delta=contentStashed->y-contentY;
      const auto regionTop=std::floor(y+contentYoga->getComputedTop());
      const auto regionBottom=regionTop+innerHeight-1;
      if(stashed&&stashed->y==y&&stashed->height==height&&
          innerHeight>0&&std::fabs(delta)<innerHeight){
        hint=ScrollHint{regionTop,regionBottom,delta};
        state_.scrollHint=hint;
      }else state_.layoutShifted=true;
    }
    const auto currentHeight=contentYoga->getComputedHeight();
    const auto prevHeight=contentStashed?contentStashed->height:currentHeight;
    const auto heightDelta=currentHeight-prevHeight;
    const bool safeForFastPath=!hint||heightDelta==0||
      (hint->delta>0&&heightDelta==hint->delta);
    if(!safeForFastPath)state_.scrollHint.reset();
    if(hint&&prevScreen&&safeForFastPath){
      renderScrollFast(*content,output,x,y,width,contentX,contentY,
        scrollTop,innerHeight,padTop,y1,prevScreen,hasRemovedChild,
        boxBackgroundColor,*hint);
    }else{
      const bool scrolled=contentStashed&&contentStashed->y!=contentY;
      if(scrolled&&y1&&y2)
        output.clear(detail::rect(std::floor(x),std::floor(*y1),
          std::floor(width),std::floor(*y2-*y1)));
      renderScrolledChildren(*content,output,contentX,contentY,
        hasRemovedChild,(scrolled||positionChanged)?
          terminal_screen::ScreenRef{}:prevScreen,
        scrollTop,scrollTop+innerHeight,boxBackgroundColor);
    }
    state_.nodeStash.set(*content,std::make_shared<terminal_dom::StashedLayout>(
      terminal_dom::StashedLayout{contentX,contentY,
        contentYoga->getComputedWidth(),contentYoga->getComputedHeight(),{}}));
    content->dirty=false;
  }
}
} // namespace batbox::terminal_render
