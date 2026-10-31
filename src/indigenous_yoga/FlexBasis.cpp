#include "indigenous_yoga/LayoutInternal.hpp"

namespace batbox::indigenous_yoga {
double computeFlexBasis(Node& child,FlexDirection mainAxis,
    double availableMain,double availableCross,MeasureMode crossMode,
    double ownerWidth,double ownerHeight){
  const bool sameGen=child._fbGen==currentGeneration();
  if((sameGen||!child.isDirty_)&&
      child._fbCrossMode==crossMode&&
      sameFloat(child._fbOwnerW,ownerWidth)&&
      sameFloat(child._fbOwnerH,ownerHeight)&&
      sameFloat(child._fbAvailMain,availableMain)&&
      sameFloat(child._fbAvailCross,availableCross))
    return child._fbBasis;
  const auto& style=child.style;
  const bool mainRow=isRow(mainAxis);
  const auto store=[&](double basis){
    child._fbBasis=basis;
    child._fbOwnerW=ownerWidth;
    child._fbOwnerH=ownerHeight;
    child._fbAvailMain=availableMain;
    child._fbAvailCross=availableCross;
    child._fbCrossMode=crossMode;
    child._fbGen=currentGeneration();
    return basis;
  };
  const auto explicitBasis=resolveValue(style.flexBasis,availableMain);
  if(isDefined(explicitBasis))return store(jsMax(0,explicitBasis));
  const auto mainStyleDim=mainRow?style.width:style.height;
  const auto mainOwner=mainRow?ownerWidth:ownerHeight;
  const auto resolved=resolveValue(mainStyleDim,mainOwner);
  if(isDefined(resolved))return store(jsMax(0,resolved));

  const auto crossStyleDim=mainRow?style.height:style.width;
  const auto crossOwner=mainRow?ownerHeight:ownerWidth;
  auto crossConstraint=resolveValue(crossStyleDim,crossOwner);
  auto crossConstraintMode=isDefined(crossConstraint)?
    MeasureMode::Exactly:MeasureMode::Undefined;
  if(!isDefined(crossConstraint)&&isDefined(availableCross)){
    crossConstraint=availableCross;
    crossConstraintMode=crossMode==MeasureMode::Exactly&&
      isStretchAlign(child)?MeasureMode::Exactly:MeasureMode::AtMost;
  }
  double mainConstraint=NaN;
  auto mainConstraintMode=MeasureMode::Undefined;
  if(mainRow&&isDefined(availableMain)&&
      hasMeasureFuncInSubtree(child)){
    mainConstraint=availableMain;
    mainConstraintMode=MeasureMode::AtMost;
  }
  const auto measuredWidth=mainRow?mainConstraint:crossConstraint;
  const auto measuredHeight=mainRow?crossConstraint:mainConstraint;
  const auto widthMode=mainRow?mainConstraintMode:crossConstraintMode;
  const auto heightMode=mainRow?crossConstraintMode:mainConstraintMode;
  layoutNode(child,measuredWidth,measuredHeight,widthMode,heightMode,
    ownerWidth,ownerHeight,false);
  const auto basis=mainRow?child.layout.width:child.layout.height;
  return store(basis);
}
} // namespace batbox::indigenous_yoga
