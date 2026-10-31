#include "interactive/transcript_navigation/Navigation.hpp"
#include "interactive/message_list_view/Search.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace batbox::interactive::transcript_navigation {
struct Navigation::Warm {
  event_loop::Deferred<double> result;
  RenderItemsRef items;
  std::size_t position=0;
  double work=0,wallStart=0;
  Warm(event_loop::Loop& owner,RenderItemsRef rows,double start):
    result(owner),items(std::move(rows)),wallStart(start){}
};
namespace {
template<class T>void need(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Transcript navigation binding required: ")+name);
}
}
Navigation::Navigation(Services services):services_(std::move(services)){
  need(services_.extractSearchText,"extractSearchText");
  need(services_.scanItem,"scanItem");
  need(services_.setPositions,"setPositions");
  need(services_.onMatchesChanged,"onMatchesChanged");
  need(services_.setStickyPoetry,"setStickyPrompt");
  need(services_.scrollToMounted,"scrollToMounted");
  need(services_.performanceNow,"performanceNow");
  need(services_.logForDebugging,"logForDebugging");
}
void Navigation::log(const std::string& message)const{
  services_.logForDebugging(message);
}
void Navigation::setItems(RenderItemsRef items){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("Transcript navigation owner required");
  items_=std::move(items);
}
double Navigation::targetFor(std::size_t index)const{
  return std::max(0.0,services_.viewport.ranges().itemTop(index)-3.0);
}
void Navigation::jumpToIndex(std::size_t index){
  services_.viewport.scrollTo(targetFor(index));
}
void Navigation::setAnchor(){searchAnchor_=services_.viewport.scroll().top;}
void Navigation::setSearchQuery(const std::string& query){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("Transcript search owner required");
  query_=query;
  seek_.reset();
  positions_.clear();positionMessage_=-1;startPtr_=-1;
  services_.setPositions({});
  const auto lower=message_list_view::lowercase(query);
  const auto needle=value::utf16CodeUnits(lower);
  std::vector<std::size_t> matches,prefix{0};
  if(!needle.empty()&&items_){
    for(std::size_t index=0;index<items_->size();++index){
      const auto text=value::utf16CodeUnits(
        services_.extractSearchText(items_->at(index)));
      std::size_t count=0;
      auto pos=text.find(needle);
      while(pos!=std::u16string::npos){
        ++count;
        pos=text.find(needle,pos+needle.size());
      }
      if(count){matches.push_back(index);prefix.push_back(prefix.back()+count);}
    }
  }
  const auto total=prefix.back();
  std::size_t ptr=0;
  const auto& ranges=services_.viewport.ranges();
  const auto first=ranges.itemTop(services_.viewport.visible().mounted.start);
  const auto origin=first>=0?
    first-ranges.offset(services_.viewport.visible().mounted.start):0.0;
  if(!matches.empty()){
    const auto current=searchAnchor_>=0?searchAnchor_:
      services_.viewport.scroll().top;
    double best=std::numeric_limits<double>::infinity();
    for(std::size_t k=0;k<matches.size();++k){
      const auto distance=std::abs(origin+ranges.offset(matches[k])-current);
      if(distance<=best){best=distance;ptr=k;}
    }
    log("setSearchQuery('"+query+"'): "+std::to_string(matches.size())+
      " msgs · ptr="+std::to_string(ptr)+" msgIdx="+
      std::to_string(matches[ptr])+" curTop="+value::numberToString(current)+
      " origin="+value::numberToString(origin));
  }
  matches_=std::move(matches);
  prefixSum_=std::move(prefix);
  ptr_=ptr;screenOrd_=0;
  if(!matches_.empty())jump(matches_[ptr_],true);
  else if(searchAnchor_>=0)services_.viewport.scrollTo(searchAnchor_);
  services_.onMatchesChanged(total,
    !matches_.empty()?prefixSum_[ptr_+1]:0);
}
void Navigation::nextMatch(){step(1);}
void Navigation::prevMatch(){step(-1);}
void Navigation::disarmSearch(){
  services_.setPositions({});
  seek_.reset();positions_.clear();positionMessage_=-1;startPtr_=-1;
}
event_loop::Deferred<double> Navigation::warmSearchIndex(){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("Transcript warm-index owner required");
  if(indexWarmed_){event_loop::Deferred<double> ready(services_.owner);
    ready.resolve(0);return ready;}
  auto state=std::make_shared<Warm>(services_.owner,items_,
    services_.performanceNow());
  warmChunk(state);
  return state->result;
}
void Navigation::warmChunk(const std::shared_ptr<Warm>& state){
  const auto length=state->items?state->items->size():0;
  if(state->position>=length){
    const auto wall=value::round(services_.performanceNow()-state->wallStart);
    log("warmSearchIndex: "+std::to_string(length)+
      " msgs · work="+value::numberToString(value::round(state->work))+
      "ms wall="+value::numberToString(wall)+"ms chunks="+
      std::to_string((length+499)/500));
    indexWarmed_=true;
    state->result.resolve(value::round(state->work));
    return;
  }
  services_.owner.setTimeout(0,[self=shared_from_this(),state]{
    try{
      const double start=self->services_.performanceNow();
      const auto end=std::min(state->position+std::size_t{500},
        state->items->size());
      for(;state->position<end;++state->position)
        self->services_.extractSearchText(state->items->at(state->position));
      state->work+=self->services_.performanceNow()-start;
      self->warmChunk(state);
    }catch(...){state->result.reject(std::current_exception());}
  });
}
} // namespace batbox::interactive::transcript_navigation
