#include "interactive/viewport/Range.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace batbox::interactive::viewport {
RangeResult RangeModel::prepare(int columns,const ScrollSnapshot& scroll,std::optional<Range> deferred){
  if(columns<=0)throw std::invalid_argument("Positive virtual viewport width required");
  if(columns_==0)columns_=columns;
  if(columns_!=columns){
    const double ratio=static_cast<double>(columns_)/columns;columns_=columns;
    for(auto& [key,height]:heights_)height=std::max(1.0,std::floor(height*ratio+0.5));
    ++version_;skipMeasurement_=true;freezeRenders_=2;
  }
  const auto frozen=freezeRenders_>0?previousRange_:std::nullopt;
  rebuildOffsets();const auto n=keys_.size();const auto total=offsets_[n];
  std::size_t start=0,end=0;
  const auto height=[&](std::size_t i){return itemHeight(i).value_or(1);};
  if(frozen){start=std::min(frozen->start,n);end=std::min(frozen->end,n);}
  else if(scroll.height==0||scroll.top<0){start=n>30?n-30:0;end=n;}
  else{
    if(scroll.sticky){
      const auto budget=scroll.height+80;start=n;
      while(start>0&&total-offsets_[start-1]<budget)--start;
      end=n;
    }else{
      const double maxSpan=scroll.height*3;
      const auto rawLo=std::min(scroll.top,scroll.top+scroll.pendingDelta);
      const auto rawHi=std::max(scroll.top,scroll.top+scroll.pendingDelta);
      const auto span=rawHi-rawLo;
      const auto clampedLo=span>maxSpan?(scroll.pendingDelta<0?rawHi-maxSpan:rawLo):rawLo;
      const auto clampedHi=clampedLo+std::min(span,maxSpan);
      const auto effLo=std::max(0.0,clampedLo-listOrigin_),effHi=clampedHi-listOrigin_;
      const auto lo=effLo-80;
      std::size_t left=0,right=n;
      while(left<right){const auto middle=(left+right)/2;if(offsets_[middle+1]<=lo)left=middle+1;else right=middle;}
      start=left;
      if(previousRange_&&previousRange_->start<start)
        for(auto i=previousRange_->start;i<std::min(start,previousRange_->end)&&i<n;++i)
          if(mounted_.contains(keys_[i])&&!heights_.contains(keys_[i])){start=i;break;}
      const auto needed=scroll.height+160;
      const auto maxEnd=std::min(n,start+300);double coverage=0;end=start;
      while(end<maxEnd&&(coverage<needed||offsets_[end]<effHi+scroll.height+80))coverage+=height(end++);
    }
    const auto needed=scroll.height+160;
    const auto minStart=end>300?end-300:0;
    double coverage=0;for(auto i=start;i<end;++i)coverage+=height(i);
    while(start>minStart&&coverage<needed)coverage+=height(--start);
    const auto velocity=std::abs(scroll.top-lastScrollTop_)+std::abs(scroll.pendingDelta);
    if(previousRange_&&velocity>scroll.height*2){
      const auto previous=*previousRange_;
      const auto earliest=previous.start>25?previous.start-25:0;
      if(start<earliest)start=earliest;
      if(end>previous.end+25)end=std::min(n,previous.end+25);
      if(start>end)end=std::min(start+25,n);
    }
    lastScrollTop_=scroll.top;
  }
  if(freezeRenders_>0)--freezeRenders_;else previousRange_=Range{start,end};
  const auto lag=deferred.value_or(Range{start,end});
  auto effStart=start<lag.start?lag.start:start;
  auto effEnd=end>lag.end?lag.end:end;
  if(effStart>effEnd||scroll.sticky){effStart=start;effEnd=end;}
  if(scroll.pendingDelta>0)effEnd=end;
  if(effEnd-effStart>300){
    const auto middle=(offsets_[effStart]+offsets_[effEnd])/2;
    if(scroll.top-listOrigin_<middle)effEnd=effStart+300;else effStart=effEnd-300;
  }
  RangeResult result;
  result.mounted={effStart,effEnd};result.immediate={start,end};
  result.topSpacer=offsets_[effStart];result.bottomSpacer=total-offsets_[effEnd];result.totalHeight=total;
  if(!scroll.sticky){
    result.clampMin=effStart==0?0:result.topSpacer+listOrigin_;
    result.clampMax=effEnd==n?std::numeric_limits<double>::infinity():
      std::max(result.topSpacer,offsets_[effEnd]-scroll.height)+listOrigin_;
  }
  return result;
}
} // namespace batbox::interactive::viewport
