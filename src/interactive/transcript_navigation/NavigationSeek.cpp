#include "interactive/transcript_navigation/Navigation.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::interactive::transcript_navigation {
void Navigation::jump(std::size_t index,bool wantLast){
  if(!items_||index>=items_->size())return;
  services_.setPositions({});
  positionMessage_=-1;positions_.clear();
  seek_=Seek{index,wantLast,0};
  const auto geometry=services_.viewport.ranges().itemGeometry(index);
  if(geometry&&geometry->height>0)
    services_.viewport.scrollTo(targetFor(index));
  else services_.viewport.scrollToIndex(index);
}
void Navigation::highlight(std::size_t ordinal){
  if(positions_.empty()||positionMessage_<0){
    services_.setPositions({});return;
  }
  const auto index=std::min(ordinal,positions_.size()-1);
  const auto& position=positions_[index];
  const auto top=services_.viewport.ranges().itemTop(
    static_cast<std::size_t>(positionMessage_));
  const auto viewportTop=services_.viewport.viewportTop();
  double relative=top-services_.viewport.scroll().top;
  const auto height=services_.viewport.scroll().height;
  double screenRow=viewportTop+relative+position.row;
  if(screenRow<viewportTop||screenRow>=viewportTop+height){
    services_.viewport.scrollTo(std::max(0.0,top+position.row-3));
    relative=top-services_.viewport.scroll().top;
    screenRow=viewportTop+relative+position.row;
  }
  services_.setPositions(PositionOverlay{
    positions_,viewportTop+relative,index});
  const auto total=prefixSum_.empty()?0:prefixSum_.back();
  const auto current=(ptr_<prefixSum_.size()?prefixSum_[ptr_]:0)+index+1;
  services_.onMatchesChanged(total,current);
  log("highlight(i="+std::to_string(positionMessage_)+", ord="+
    std::to_string(index)+"/"+std::to_string(positions_.size())+
    "): pos={row:"+value::numberToString(position.row)+",col:"+
    value::numberToString(position.col)+"} lo="+
    value::numberToString(relative)+" screenRow="+
    value::numberToString(screenRow)+" badge="+
    std::to_string(current)+"/"+std::to_string(total));
}
void Navigation::step(int delta){
  if(matches_.empty())return;
  if(seek_){pendingStep_=delta;return;}
  if(startPtr_<0)startPtr_=static_cast<std::ptrdiff_t>(ptr_);
  const auto nextOrdinal=static_cast<std::ptrdiff_t>(screenOrd_)+delta;
  if(nextOrdinal>=0&&
      nextOrdinal<static_cast<std::ptrdiff_t>(positions_.size())){
    screenOrd_=static_cast<std::size_t>(nextOrdinal);
    highlight(screenOrd_);
    startPtr_=-1;return;
  }
  const auto next=(static_cast<std::ptrdiff_t>(ptr_)+delta+
    static_cast<std::ptrdiff_t>(matches_.size()))%
    static_cast<std::ptrdiff_t>(matches_.size());
  if(next==startPtr_){
    services_.setPositions({});
    startPtr_=-1;
    log("step: wraparound at ptr="+std::to_string(next)+
      ", all "+std::to_string(matches_.size())+" msgs phantoms");
    return;
  }
  ptr_=static_cast<std::size_t>(next);
  screenOrd_=0;
  jump(matches_[ptr_],delta<0);
  const auto total=prefixSum_.back();
  const auto placeholder=delta<0?prefixSum_[ptr_+1]:prefixSum_[ptr_]+1;
  services_.onMatchesChanged(total,placeholder);
}
void Navigation::afterPaint(){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("Transcript paint owner required");
  if(!seek_)return;
  const auto request=*seek_;
  const auto scanned=services_.scanItem(request.index,query_);
  if(!scanned){
    if(request.tries>1){
      seek_.reset();
      log("seek(i="+std::to_string(request.index)+
        "): no mount after scrollToIndex, skip");
      step(request.wantLast?-1:1);
      return;
    }
    seek_->tries++;
    services_.viewport.scrollToIndex(request.index);
    return;
  }
  seek_.reset();
  services_.viewport.scrollTo(targetFor(request.index));
  positionMessage_=static_cast<std::ptrdiff_t>(request.index);
  positions_=*scanned;
  log("seek(i="+std::to_string(request.index)+" t="+
    std::to_string(request.tries)+"): "+
    std::to_string(positions_.size())+" positions");
  if(positions_.empty()){
    if(++phantomBurst_>20){phantomBurst_=0;return;}
    step(request.wantLast?-1:1);
    return;
  }
  phantomBurst_=0;
  const auto ordinal=request.wantLast?positions_.size()-1:0;
  screenOrd_=ordinal;
  startPtr_=-1;
  highlight(ordinal);
  if(pendingStep_){
    const auto pending=pendingStep_;pendingStep_=0;step(pending);
  }
}
} // namespace batbox::interactive::transcript_navigation
