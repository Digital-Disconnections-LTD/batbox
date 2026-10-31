#include "indigenous_yoga/LayoutInternal.hpp"

namespace batbox::indigenous_yoga {
void resolveFlexibleLengths(const NodeList& children,
    double availableInnerMain,double totalFlexBasis,bool mainRow,
    double ownerW,double ownerH){
  const auto count=children.size();
  std::vector<bool> frozen(count,false);
  const auto initialFree=isDefined(availableInnerMain)?
    availableInnerMain-totalFlexBasis:0;
  for(std::size_t i=0;i<count;++i){
    auto& child=*children[i];
    const auto clamped=boundAxis(child.style,mainRow,child._flexBasis,
      ownerW,ownerH);
    const bool inflexible=!isDefined(availableInnerMain)||
      (initialFree>=0?child.style.flexGrow==0:child.style.flexShrink==0);
    if(inflexible){
      child._mainSize=jsMax(0,clamped);frozen[i]=true;
    }else child._mainSize=child._flexBasis;
  }
  std::vector<double> unclamped(count);
  for(std::size_t iteration=0;iteration<=count;++iteration){
    double frozenDelta=0,totalGrow=0,totalShrinkScaled=0;
    std::size_t unfrozenCount=0;
    for(std::size_t i=0;i<count;++i){
      const auto& child=*children[i];
      if(frozen[i])frozenDelta+=child._mainSize-child._flexBasis;
      else{
        totalGrow+=child.style.flexGrow;
        totalShrinkScaled+=child.style.flexShrink*child._flexBasis;
        ++unfrozenCount;
      }
    }
    if(unfrozenCount==0)break;
    auto remaining=initialFree-frozenDelta;
    if(remaining>0&&totalGrow>0&&totalGrow<1){
      const auto scaled=initialFree*totalGrow;
      if(scaled<remaining)remaining=scaled;
    }else if(remaining<0&&totalShrinkScaled>0){
      double totalShrink=0;
      for(std::size_t i=0;i<count;++i)
        if(!frozen[i])totalShrink+=children[i]->style.flexShrink;
      if(totalShrink<1){
        const auto scaled=initialFree*totalShrink;
        if(scaled>remaining)remaining=scaled;
      }
    }
    double totalViolation=0;
    for(std::size_t i=0;i<count;++i){
      if(frozen[i])continue;
      auto& child=*children[i];
      auto target=child._flexBasis;
      if(remaining>0&&totalGrow>0)
        target+=remaining*child.style.flexGrow/totalGrow;
      else if(remaining<0&&totalShrinkScaled>0)
        target+=remaining*(child.style.flexShrink*child._flexBasis)/
          totalShrinkScaled;
      unclamped[i]=target;
      const auto clamped=jsMax(0,boundAxis(child.style,mainRow,target,
        ownerW,ownerH));
      child._mainSize=clamped;
      totalViolation+=clamped-target;
    }
    if(totalViolation==0)break;
    bool anyFrozen=false;
    for(std::size_t i=0;i<count;++i){
      if(frozen[i])continue;
      const auto violation=children[i]->_mainSize-unclamped[i];
      if((totalViolation>0&&violation>0)||
          (totalViolation<0&&violation<0)){
        frozen[i]=true;anyFrozen=true;
      }
    }
    if(!anyFrozen)break;
  }
}
} // namespace batbox::indigenous_yoga
