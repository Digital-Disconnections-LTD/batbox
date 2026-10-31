#include "interactive/text_input/KillRing.hpp"

namespace batbox::interactive::text_input {
void KillRing::push(std::string text,Direction direction){
  if(text.empty())return;
  if(lastActionWasKill_&&!ring_.empty()){
    if(direction==Direction::Prepend)ring_.front()=text+ring_.front();
    else ring_.front()+=text;
  }else{
    ring_.insert(ring_.begin(),std::move(text));
    if(ring_.size()>maxSize)ring_.pop_back();
  }
  lastActionWasKill_=true;
  lastActionWasYank_=false;
}
const std::string& KillRing::last()const{
  static const std::string empty;
  return ring_.empty()?empty:ring_.front();
}
std::string KillRing::item(long long index)const{
  if(ring_.empty())return {};
  const auto size=static_cast<long long>(ring_.size());
  const auto normalized=((index%size)+size)%size;
  return ring_[static_cast<std::size_t>(normalized)];
}
void KillRing::clear(){
  ring_.clear();ringIndex_=0;lastActionWasKill_=false;
  lastActionWasYank_=false;lastYankStart_=lastYankLength_=0;
}
void KillRing::recordYank(std::size_t start,std::size_t length){
  lastYankStart_=start;lastYankLength_=length;
  lastActionWasYank_=true;ringIndex_=0;
}
bool KillRing::canYankPop()const{
  return lastActionWasYank_&&ring_.size()>1;
}
std::optional<YankPop> KillRing::yankPop(){
  if(!canYankPop())return std::nullopt;
  ringIndex_=(ringIndex_+1)%ring_.size();
  return YankPop{ring_[ringIndex_],lastYankStart_,lastYankLength_};
}
} // namespace batbox::interactive::text_input
