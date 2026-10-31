#include "indigenous_yoga/LayoutInternal.hpp"

namespace batbox::indigenous_yoga {
namespace {
double justifyAbsolute(Justify justify,double lead,double trail,
    double childSize){
  switch(justify){
    case Justify::Center:return lead+(trail-lead-childSize)/2;
    case Justify::FlexEnd:return trail-childSize;
    default:return lead;
  }
}
double alignAbsolute(Align align,double lead,double trail,
    double childSize,bool wrapReverse){
  switch(align){
    case Align::Center:return lead+(trail-lead-childSize)/2;
    case Align::FlexEnd:return wrapReverse?lead:trail-childSize;
    default:return wrapReverse?trail-childSize:lead;
  }
}
}
void layoutAbsoluteChild(Node& parent,Node& child,
    double parentWidth,double parentHeight,
    const Edges& padding,const Edges& border){
  const auto& style=child.style;
  const auto posLeft=resolveEdgeRaw(style.position,0);
  const auto posRight=resolveEdgeRaw(style.position,2);
  const auto posTop=resolveEdgeRaw(style.position,1);
  const auto posBottom=resolveEdgeRaw(style.position,3);
  const auto rLeft=resolveValue(posLeft,parentWidth);
  const auto rRight=resolveValue(posRight,parentWidth);
  const auto rTop=resolveValue(posTop,parentHeight);
  const auto rBottom=resolveValue(posBottom,parentHeight);
  const auto paddingBoxW=parentWidth-border[0]-border[2];
  const auto paddingBoxH=parentHeight-border[1]-border[3];
  auto width=resolveValue(style.width,paddingBoxW);
  auto height=resolveValue(style.height,paddingBoxH);
  if(!isDefined(width)&&isDefined(rLeft)&&isDefined(rRight))
    width=paddingBoxW-rLeft-rRight;
  if(!isDefined(height)&&isDefined(rTop)&&isDefined(rBottom))
    height=paddingBoxH-rTop-rBottom;
  layoutNode(child,width,height,
    isDefined(width)?MeasureMode::Exactly:MeasureMode::Undefined,
    isDefined(height)?MeasureMode::Exactly:MeasureMode::Undefined,
    paddingBoxW,paddingBoxH,true);
  const auto marginLeft=resolveEdge(style.margin,0,parentWidth);
  const auto marginTop=resolveEdge(style.margin,1,parentWidth);
  const auto marginRight=resolveEdge(style.margin,2,parentWidth);
  const auto marginBottom=resolveEdge(style.margin,3,parentWidth);
  const auto mainAxis=parent.style.flexDirection;
  const bool reversed=isReverse(mainAxis);
  const bool mainRow=isRow(mainAxis);
  const bool wrapReverse=parent.style.flexWrap==Wrap::WrapReverse;
  const auto alignment=style.alignSelf==Align::Auto?
    parent.style.alignItems:style.alignSelf;
  double left;
  if(isDefined(rLeft))left=border[0]+rLeft+marginLeft;
  else if(isDefined(rRight))
    left=parentWidth-border[2]-rRight-child.layout.width-marginRight;
  else if(mainRow){
    const auto lead=padding[0]+border[0];
    const auto trail=parentWidth-padding[2]-border[2];
    left=reversed?trail-child.layout.width-marginRight:
      justifyAbsolute(parent.style.justifyContent,lead,trail,
        child.layout.width)+marginLeft;
  }else left=alignAbsolute(alignment,padding[0]+border[0],
      parentWidth-padding[2]-border[2],child.layout.width,
      wrapReverse)+marginLeft;
  double top;
  if(isDefined(rTop))top=border[1]+rTop+marginTop;
  else if(isDefined(rBottom))
    top=parentHeight-border[3]-rBottom-child.layout.height-marginBottom;
  else if(mainRow)
    top=alignAbsolute(alignment,padding[1]+border[1],
      parentHeight-padding[3]-border[3],child.layout.height,
      wrapReverse)+marginTop;
  else{
    const auto lead=padding[1]+border[1];
    const auto trail=parentHeight-padding[3]-border[3];
    top=reversed?trail-child.layout.height-marginBottom:
      justifyAbsolute(parent.style.justifyContent,lead,trail,
        child.layout.height)+marginTop;
  }
  child.layout.left=left;
  child.layout.top=top;
}
} // namespace batbox::indigenous_yoga
