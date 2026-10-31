#include "indigenous_yoga/LayoutInternal.hpp"

namespace batbox::indigenous_yoga {
void layoutContainer(LayoutFrame frame){
  auto& node=frame.node;
  const auto& style=node.style;
  const auto mainAxis=style.flexDirection;
  const auto crossAx=crossAxis(mainAxis);
  const bool mainRow=isRow(mainAxis);
  const auto mainSize=mainRow?frame.width:frame.height;
  const auto crossSize=mainRow?frame.height:frame.width;
  const auto mainMode=mainRow?frame.wMode:frame.hMode;
  const auto crossMode=mainRow?frame.hMode:frame.wMode;
  const auto mainPadBorder=mainRow?frame.paddingBorderWidth:
    frame.paddingBorderHeight;
  const auto crossPadBorder=mainRow?frame.paddingBorderHeight:
    frame.paddingBorderWidth;
  const auto innerMainSize=isDefined(mainSize)?
    jsMax(0,mainSize-mainPadBorder):NaN;
  const auto innerCrossSize=isDefined(crossSize)?
    jsMax(0,crossSize-crossPadBorder):NaN;
  const auto gapMain=resolveGap(style,mainRow?Gutter::Column:Gutter::Row,
    innerMainSize);
  NodeList flowChildren,absChildren;
  collectLayoutChildren(node,flowChildren,absChildren);
  const auto ownerW=isDefined(frame.width)?frame.width:NaN;
  const auto ownerH=isDefined(frame.height)?frame.height:NaN;
  const bool wrap=style.flexWrap!=Wrap::NoWrap;
  const auto gapCross=resolveGap(style,mainRow?Gutter::Row:Gutter::Column,
    innerCrossSize);
  for(auto* child:flowChildren)
    child->_flexBasis=computeFlexBasis(*child,mainAxis,
      innerMainSize,innerCrossSize,crossMode,ownerW,ownerH);
  std::vector<NodeList> lines;
  if(!wrap||!isDefined(innerMainSize)||flowChildren.empty()){
    for(auto* child:flowChildren)child->_lineIndex=0;
    lines.push_back(flowChildren);
  }else{
    std::size_t lineStart=0;
    double lineLength=0;
    for(std::size_t i=0;i<flowChildren.size();++i){
      auto& child=*flowChildren[i];
      const auto hypothetical=boundAxis(child.style,mainRow,
        child._flexBasis,ownerW,ownerH);
      const auto outer=jsMax(0,hypothetical)+
        childMarginForAxis(child,mainAxis,ownerW);
      const auto withGap=i>lineStart?gapMain:0;
      if(i>lineStart&&lineLength+withGap+outer>innerMainSize){
        lines.emplace_back(flowChildren.begin()+lineStart,
          flowChildren.begin()+i);
        lineStart=i;lineLength=outer;
      }else lineLength+=withGap+outer;
      child._lineIndex=static_cast<int>(lines.size());
    }
    lines.emplace_back(flowChildren.begin()+lineStart,flowChildren.end());
  }
  const auto lineCount=lines.size();
  const bool baseline=isBaselineLayout(node,flowChildren);
  std::vector<double> lineConsumedMain(lineCount);
  std::vector<double> lineCrossSizes(lineCount);
  std::vector<double> lineMaxAscent(baseline?lineCount:0,0);
  double maxLineMain=0,totalLinesCross=0;
  for(std::size_t li=0;li<lineCount;++li){
    const auto& line=lines[li];
    const auto lineGap=line.size()>1?gapMain*(line.size()-1):0;
    double lineBasis=lineGap;
    for(const auto* child:line)
      lineBasis+=child->_flexBasis+
        childMarginForAxis(*child,mainAxis,ownerW);
    auto availableMain=innerMainSize;
    if(!isDefined(availableMain)){
      const auto mainOwner=mainRow?frame.ownerWidth:frame.ownerHeight;
      const auto minMain=resolveValue(mainRow?style.minWidth:style.minHeight,
        mainOwner);
      const auto maxMain=resolveValue(mainRow?style.maxWidth:style.maxHeight,
        mainOwner);
      if(isDefined(maxMain)&&lineBasis>maxMain-mainPadBorder)
        availableMain=jsMax(0,maxMain-mainPadBorder);
      else if(isDefined(minMain)&&lineBasis<minMain-mainPadBorder)
        availableMain=jsMax(0,minMain-mainPadBorder);
    }
    resolveFlexibleLengths(line,availableMain,lineBasis,mainRow,ownerW,ownerH);
    double lineCross=0;
    for(auto* child:line){
      const auto& childStyle=child->style;
      const auto align=childStyle.alignSelf==Align::Auto?
        style.alignItems:childStyle.alignSelf;
      const auto marginCross=childMarginForAxis(*child,crossAx,ownerW);
      auto childCrossSize=NaN;
      auto childCrossMode=MeasureMode::Undefined;
      const auto resolvedCross=resolveValue(mainRow?childStyle.height:
        childStyle.width,mainRow?ownerH:ownerW);
      const int crossLead=mainRow?1:0;
      const int crossTrail=mainRow?3:2;
      const bool hasCrossAuto=child->_hasAutoMargin&&
        (isMarginAuto(childStyle.margin,crossLead)||
         isMarginAuto(childStyle.margin,crossTrail));
      if(isDefined(resolvedCross)){
        childCrossSize=resolvedCross;
        childCrossMode=MeasureMode::Exactly;
      }else if(align==Align::Stretch&&!hasCrossAuto&&!wrap&&
          isDefined(innerCrossSize)&&crossMode==MeasureMode::Exactly){
        childCrossSize=jsMax(0,innerCrossSize-marginCross);
        childCrossMode=MeasureMode::Exactly;
      }else if(!wrap&&isDefined(innerCrossSize)){
        childCrossSize=jsMax(0,innerCrossSize-marginCross);
        childCrossMode=MeasureMode::AtMost;
      }
      const auto childWidth=mainRow?child->_mainSize:childCrossSize;
      const auto childHeight=mainRow?childCrossSize:child->_mainSize;
      layoutNode(*child,childWidth,childHeight,
        mainRow?MeasureMode::Exactly:childCrossMode,
        mainRow?childCrossMode:MeasureMode::Exactly,
        ownerW,ownerH,frame.performLayout,mainRow,!mainRow);
      child->_crossSize=mainRow?child->layout.height:child->layout.width;
      lineCross=jsMax(lineCross,child->_crossSize+marginCross);
    }
    if(baseline){
      double maxAscent=0,maxDescent=0;
      for(const auto* child:line){
        if(resolveChildAlign(node,*child)!=Align::Baseline)continue;
        const auto marginTop=resolveEdge(child->style.margin,1,ownerW);
        const auto marginBottom=resolveEdge(child->style.margin,3,ownerW);
        const auto ascent=calculateBaseline(*child)+marginTop;
        const auto descent=child->layout.height+marginTop+marginBottom-ascent;
        if(ascent>maxAscent)maxAscent=ascent;
        if(descent>maxDescent)maxDescent=descent;
      }
      lineMaxAscent[li]=maxAscent;
      if(maxAscent+maxDescent>lineCross)
        lineCross=maxAscent+maxDescent;
    }
    const auto mainLead=leadingEdge(mainAxis);
    const auto mainTrail=trailingEdge(mainAxis);
    auto consumed=lineGap;
    for(const auto* child:line){
      const auto& margin=child->layout.margin;
      consumed+=child->_mainSize+margin[mainLead]+margin[mainTrail];
    }
    lineConsumedMain[li]=consumed;
    lineCrossSizes[li]=lineCross;
    maxLineMain=jsMax(maxLineMain,consumed);
    totalLinesCross+=lineCross;
  }
  const auto totalCrossGap=lineCount>1?gapCross*(lineCount-1):0;
  totalLinesCross+=totalCrossGap;
  const bool scroll=style.overflow==Overflow::Scroll;
  const auto contentMain=maxLineMain+mainPadBorder;
  const auto finalMain=mainMode==MeasureMode::Exactly?mainSize:
    mainMode==MeasureMode::AtMost&&scroll?
      jsMax(jsMin(mainSize,contentMain),mainPadBorder):
    wrap&&lineCount>1&&mainMode==MeasureMode::AtMost?
      mainSize:contentMain;
  const auto contentCross=totalLinesCross+crossPadBorder;
  const auto finalCross=crossMode==MeasureMode::Exactly?crossSize:
    crossMode==MeasureMode::AtMost&&scroll?
      jsMax(jsMin(crossSize,contentCross),crossPadBorder):contentCross;
  node.layout.width=boundAxis(style,true,
    mainRow?finalMain:finalCross,frame.ownerWidth,frame.ownerHeight);
  node.layout.height=boundAxis(style,false,
    mainRow?finalCross:finalMain,frame.ownerWidth,frame.ownerHeight);
  commitStashOutputs(node,frame.performLayout);
  stashWrite(node,frame.availableWidth,frame.availableHeight,
    frame.widthMode,frame.heightMode,frame.ownerWidth,frame.ownerHeight,
    frame.forceWidth,frame.forceHeight,frame.wasDirty);
  if(!frame.performLayout)return;
  positionContainer({node,lines,absChildren,lineConsumedMain,lineCrossSizes,
    lineMaxAscent,totalLinesCross,gapMain,gapCross,mainPadBorder,
    crossPadBorder,ownerW,ownerH,mainRow,wrap,baseline,crossMode});
}
} // namespace batbox::indigenous_yoga
