#include "indigenous_yoga/LayoutInternal.hpp"

namespace batbox::indigenous_yoga {
void positionContainer(PositionFrame frame){
  auto& node=frame.node;
  const auto& style=node.style;
  const auto& padding=node.layout.padding;
  const auto& border=node.layout.border;
  const auto mainAxis=style.flexDirection;
  const auto crossAx=crossAxis(mainAxis);
  const auto actualInnerMain=(frame.mainRow?node.layout.width:
    node.layout.height)-frame.mainPadBorder;
  const auto actualInnerCross=(frame.mainRow?node.layout.height:
    node.layout.width)-frame.crossPadBorder;
  const auto mainLeadEdge=leadingEdge(mainAxis);
  const auto mainTrailEdge=trailingEdge(mainAxis);
  const int crossLeadEdge=frame.mainRow?1:0;
  const int crossTrailEdge=frame.mainRow?3:2;
  const bool reversed=isReverse(mainAxis);
  const auto mainContainerSize=frame.mainRow?node.layout.width:
    node.layout.height;
  const auto crossLead=padding[crossLeadEdge]+border[crossLeadEdge];
  auto lineCrossOffset=crossLead;
  auto betweenLines=frame.gapCross;
  const auto freeCross=actualInnerCross-frame.totalLinesCross;
  const auto lineCount=frame.lines.size();
  if(lineCount==1&&!frame.wrap&&!frame.baseline)
    frame.lineCrossSizes[0]=actualInnerCross;
  else{
    const auto remainingCross=jsMax(0,freeCross);
    switch(style.alignContent){
      case Align::FlexStart:break;
      case Align::Center:lineCrossOffset+=freeCross/2;break;
      case Align::FlexEnd:lineCrossOffset+=freeCross;break;
      case Align::Stretch:
        if(lineCount>0&&remainingCross>0){
          const auto add=remainingCross/lineCount;
          for(std::size_t i=0;i<lineCount;++i)
            frame.lineCrossSizes[i]+=add;
        }
        break;
      case Align::SpaceBetween:
        if(lineCount>1)betweenLines+=remainingCross/(lineCount-1);
        break;
      case Align::SpaceAround:
        if(lineCount>0){
          betweenLines+=remainingCross/lineCount;
          lineCrossOffset+=remainingCross/lineCount/2;
        }
        break;
      case Align::SpaceEvenly:
        if(lineCount>0){
          betweenLines+=remainingCross/(lineCount+1);
          lineCrossOffset+=remainingCross/(lineCount+1);
        }
        break;
      default:break;
    }
  }
  const bool wrapReverse=style.flexWrap==Wrap::WrapReverse;
  const auto crossContainerSize=frame.mainRow?node.layout.height:
    node.layout.width;
  auto lineCrossPos=lineCrossOffset;
  for(std::size_t li=0;li<lineCount;++li){
    const auto& line=frame.lines[li];
    const auto lineCross=frame.lineCrossSizes[li];
    const auto consumedMain=frame.lineConsumedMain[li];
    const auto count=line.size();
    if(frame.wrap||frame.crossMode!=MeasureMode::Exactly){
      for(auto* child:line){
        const auto& childStyle=child->style;
        const auto align=childStyle.alignSelf==Align::Auto?
          style.alignItems:childStyle.alignSelf;
        const bool crossStyleDefined=isDefined(resolveValue(
          frame.mainRow?childStyle.height:childStyle.width,
          frame.mainRow?frame.ownerH:frame.ownerW));
        const bool crossAutoMargin=child->_hasAutoMargin&&
          (isMarginAuto(childStyle.margin,crossLeadEdge)||
           isMarginAuto(childStyle.margin,crossTrailEdge));
        if(align==Align::Stretch&&!crossStyleDefined&&!crossAutoMargin){
          const auto marginCross=childMarginForAxis(*child,crossAx,
            frame.ownerW);
          const auto target=jsMax(0,lineCross-marginCross);
          if(child->_crossSize!=target){
            const auto childWidth=frame.mainRow?child->_mainSize:target;
            const auto childHeight=frame.mainRow?target:child->_mainSize;
            layoutNode(*child,childWidth,childHeight,
              MeasureMode::Exactly,MeasureMode::Exactly,
              frame.ownerW,frame.ownerH,true,
              frame.mainRow,!frame.mainRow);
            child->_crossSize=target;
          }
        }
      }
    }
    auto mainOffset=padding[mainLeadEdge]+border[mainLeadEdge];
    auto betweenMain=frame.gapMain;
    int autoMarginsMain=0;
    for(const auto* child:line){
      if(!child->_hasAutoMargin)continue;
      if(isMarginAuto(child->style.margin,mainLeadEdge))++autoMarginsMain;
      if(isMarginAuto(child->style.margin,mainTrailEdge))++autoMarginsMain;
    }
    const auto freeMain=actualInnerMain-consumedMain;
    const auto remainingMain=jsMax(0,freeMain);
    const auto autoMarginMainSize=autoMarginsMain>0&&remainingMain>0?
      remainingMain/autoMarginsMain:0;
    if(autoMarginsMain==0){
      switch(style.justifyContent){
        case Justify::FlexStart:break;
        case Justify::Center:mainOffset+=freeMain/2;break;
        case Justify::FlexEnd:mainOffset+=freeMain;break;
        case Justify::SpaceBetween:
          if(count>1)betweenMain+=remainingMain/(count-1);
          break;
        case Justify::SpaceAround:
          if(count>0){
            betweenMain+=remainingMain/count;
            mainOffset+=remainingMain/count/2;
          }
          break;
        case Justify::SpaceEvenly:
          if(count>0){
            betweenMain+=remainingMain/(count+1);
            mainOffset+=remainingMain/(count+1);
          }
          break;
      }
    }
    const auto effectiveLineCrossPos=wrapReverse?
      crossContainerSize-lineCrossPos-lineCross:lineCrossPos;
    auto position=mainOffset;
    for(auto* child:line){
      const auto& childMargin=child->style.margin;
      const auto& resolvedMargin=child->layout.margin;
      bool autoMainLead=false,autoMainTrail=false;
      bool autoCrossLead=false,autoCrossTrail=false;
      double marginMainLead,marginMainTrail,marginCrossLead,marginCrossTrail;
      if(child->_hasAutoMargin){
        autoMainLead=isMarginAuto(childMargin,mainLeadEdge);
        autoMainTrail=isMarginAuto(childMargin,mainTrailEdge);
        autoCrossLead=isMarginAuto(childMargin,crossLeadEdge);
        autoCrossTrail=isMarginAuto(childMargin,crossTrailEdge);
        marginMainLead=autoMainLead?autoMarginMainSize:
          resolvedMargin[mainLeadEdge];
        marginMainTrail=autoMainTrail?autoMarginMainSize:
          resolvedMargin[mainTrailEdge];
        marginCrossLead=autoCrossLead?0:resolvedMargin[crossLeadEdge];
        marginCrossTrail=autoCrossTrail?0:resolvedMargin[crossTrailEdge];
      }else{
        marginMainLead=resolvedMargin[mainLeadEdge];
        marginMainTrail=resolvedMargin[mainTrailEdge];
        marginCrossLead=resolvedMargin[crossLeadEdge];
        marginCrossTrail=resolvedMargin[crossTrailEdge];
      }
      const auto mainPosition=reversed?
        mainContainerSize-(position+marginMainLead)-child->_mainSize:
        position+marginMainLead;
      const auto align=child->style.alignSelf==Align::Auto?
        style.alignItems:child->style.alignSelf;
      auto crossPosition=effectiveLineCrossPos+marginCrossLead;
      const auto crossFree=lineCross-child->_crossSize-
        marginCrossLead-marginCrossTrail;
      if(autoCrossLead&&autoCrossTrail)
        crossPosition+=jsMax(0,crossFree)/2;
      else if(autoCrossLead)crossPosition+=jsMax(0,crossFree);
      else if(autoCrossTrail){}
      else{
        switch(align){
          case Align::FlexStart:case Align::Stretch:
            if(wrapReverse)crossPosition+=crossFree;
            break;
          case Align::Center:crossPosition+=crossFree/2;break;
          case Align::FlexEnd:
            if(!wrapReverse)crossPosition+=crossFree;
            break;
          case Align::Baseline:
            if(frame.baseline)
              crossPosition=effectiveLineCrossPos+
                frame.lineMaxAscent[li]-calculateBaseline(*child);
            break;
          default:break;
        }
      }
      double relativeX=0,relativeY=0;
      if(child->_hasPosition){
        const auto relLeft=resolveValue(
          resolveEdgeRaw(child->style.position,0),frame.ownerW);
        const auto relRight=resolveValue(
          resolveEdgeRaw(child->style.position,2),frame.ownerW);
        const auto relTop=resolveValue(
          resolveEdgeRaw(child->style.position,1),frame.ownerW);
        const auto relBottom=resolveValue(
          resolveEdgeRaw(child->style.position,3),frame.ownerW);
        relativeX=isDefined(relLeft)?relLeft:
          isDefined(relRight)?-relRight:0;
        relativeY=isDefined(relTop)?relTop:
          isDefined(relBottom)?-relBottom:0;
      }
      if(frame.mainRow){
        child->layout.left=mainPosition+relativeX;
        child->layout.top=crossPosition+relativeY;
      }else{
        child->layout.left=crossPosition+relativeX;
        child->layout.top=mainPosition+relativeY;
      }
      position+=child->_mainSize+marginMainLead+marginMainTrail+betweenMain;
    }
    lineCrossPos+=lineCross+betweenLines;
  }
  for(auto* child:frame.absoluteChildren)
    layoutAbsoluteChild(node,*child,node.layout.width,node.layout.height,
      padding,border);
}
} // namespace batbox::indigenous_yoga
