#include "indigenous_yoga/LayoutInternal.hpp"

namespace batbox::indigenous_yoga {
void layoutNode(Node& node,double availableWidth,double availableHeight,
    MeasureMode widthMode,MeasureMode heightMode,double ownerWidth,
    double ownerHeight,bool performLayout,bool forceWidth,bool forceHeight){
  incrementNodesVisited();
  const auto& style=node.style;
  auto& layout=node.layout;
  const bool sameGen=node._cGen==currentGeneration()&&!performLayout;
  if(!node.isDirty_||sameGen){
    if(!node.isDirty_&&node._hasL&&
        node._lWM==widthMode&&node._lHM==heightMode&&
        node._lFW==forceWidth&&node._lFH==forceHeight&&
        sameFloat(node._lW,availableWidth)&&
        sameFloat(node._lH,availableHeight)&&
        sameFloat(node._lOW,ownerWidth)&&
        sameFloat(node._lOH,ownerHeight)){
      incrementStashHits();
      layout.width=node._lOutW;layout.height=node._lOutH;
      return;
    }
    if(node._cN>0&&(sameGen||!node.isDirty_)){
      if(!node._stashAllocated)
        throw TypeError("null is not an object (evaluating 'cIn[o + 2]')");
      for(int i=0;i<node._cN;++i){
        const int offset=i*8;
        const auto& input=node._cIn;
        if(input[offset+2]==int(widthMode)&&
            input[offset+3]==int(heightMode)&&
            input[offset+6]==(forceWidth?1:0)&&
            input[offset+7]==(forceHeight?1:0)&&
            sameFloat(input[offset],availableWidth)&&
            sameFloat(input[offset+1],availableHeight)&&
            sameFloat(input[offset+4],ownerWidth)&&
            sameFloat(input[offset+5],ownerHeight)){
          layout.width=node._cOut[i*2];
          layout.height=node._cOut[i*2+1];
          incrementStashHits();
          return;
        }
      }
    }
    if(!node.isDirty_&&!performLayout&&node._hasM&&
        node._mWM==widthMode&&node._mHM==heightMode&&
        sameFloat(node._mW,availableWidth)&&
        sameFloat(node._mH,availableHeight)&&
        sameFloat(node._mOW,ownerWidth)&&
        sameFloat(node._mOH,ownerHeight)){
      layout.width=node._mOutW;layout.height=node._mOutH;
      incrementStashHits();return;
    }
  }
  const bool wasDirty=node.isDirty_;
  if(performLayout){
    node._lW=availableWidth;node._lH=availableHeight;
    node._lWM=widthMode;node._lHM=heightMode;
    node._lOW=ownerWidth;node._lOH=ownerHeight;
    node._lFW=forceWidth;node._lFH=forceHeight;
    node._hasL=true;node.isDirty_=false;
    if(wasDirty)node._hasM=false;
  }else{
    node._mW=availableWidth;node._mH=availableHeight;
    node._mWM=widthMode;node._mHM=heightMode;
    node._mOW=ownerWidth;node._mOH=ownerHeight;
    node._hasM=true;
    if(wasDirty)node._hasL=false;
  }
  auto& padding=layout.padding;
  auto& border=layout.border;
  auto& margin=layout.margin;
  if(node._hasPadding)resolveEdges4Into(style.padding,ownerWidth,padding);
  else padding={};
  if(node._hasBorder)resolveEdges4Into(style.border,ownerWidth,border);
  else border={};
  if(node._hasMargin)resolveEdges4Into(style.margin,ownerWidth,margin);
  else margin={};
  const auto paddingBorderWidth=padding[0]+padding[2]+border[0]+border[2];
  const auto paddingBorderHeight=padding[1]+padding[3]+border[1]+border[3];
  const auto styleWidth=forceWidth?NaN:resolveValue(style.width,ownerWidth);
  const auto styleHeight=forceHeight?NaN:resolveValue(style.height,ownerHeight);
  auto width=availableWidth,height=availableHeight;
  auto wMode=widthMode,hMode=heightMode;
  if(isDefined(styleWidth)){width=styleWidth;wMode=MeasureMode::Exactly;}
  if(isDefined(styleHeight)){height=styleHeight;hMode=MeasureMode::Exactly;}
  width=boundAxis(style,true,width,ownerWidth,ownerHeight);
  height=boundAxis(style,false,height,ownerWidth,ownerHeight);
  if(node.measureFunc&&node.children.empty()){
    const auto innerW=wMode==MeasureMode::Undefined?NaN:
      jsMax(0,width-paddingBorderWidth);
    const auto innerH=hMode==MeasureMode::Undefined?NaN:
      jsMax(0,height-paddingBorderHeight);
    incrementMeasureCalls();
    const auto measured=node.measureFunc(innerW,wMode,innerH,hMode);
    layout.width=wMode==MeasureMode::Exactly?width:
      boundAxis(style,true,measured.width.value_or(0)+paddingBorderWidth,
        ownerWidth,ownerHeight);
    layout.height=hMode==MeasureMode::Exactly?height:
      boundAxis(style,false,measured.height.value_or(0)+paddingBorderHeight,
        ownerWidth,ownerHeight);
    commitStashOutputs(node,performLayout);
    stashWrite(node,availableWidth,availableHeight,widthMode,heightMode,
      ownerWidth,ownerHeight,forceWidth,forceHeight,wasDirty);
    return;
  }
  if(node.children.empty()){
    layout.width=wMode==MeasureMode::Exactly?width:
      boundAxis(style,true,paddingBorderWidth,ownerWidth,ownerHeight);
    layout.height=hMode==MeasureMode::Exactly?height:
      boundAxis(style,false,paddingBorderHeight,ownerWidth,ownerHeight);
    commitStashOutputs(node,performLayout);
    stashWrite(node,availableWidth,availableHeight,widthMode,heightMode,
      ownerWidth,ownerHeight,forceWidth,forceHeight,wasDirty);
    return;
  }
  layoutContainer({node,availableWidth,availableHeight,
    ownerWidth,ownerHeight,widthMode,heightMode,performLayout,
    forceWidth,forceHeight,wasDirty,width,height,wMode,hMode,
    paddingBorderWidth,paddingBorderHeight});
}
} // namespace batbox::indigenous_yoga
