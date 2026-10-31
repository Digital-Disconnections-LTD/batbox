#include "interactive/cards_collapsed/MinHintDisplay.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_collapsed {

std::shared_ptr<MinHintDisplay> MinHintDisplay::create(event_loop::Loop& loop,
    std::function<double()> nowMs,std::function<void()> onChanged){
  if(!nowMs||!onChanged)
    throw std::logic_error("Hint clock and view-change imports required");
  return std::shared_ptr<MinHintDisplay>(new MinHintDisplay(loop,
      std::move(nowMs),std::move(onChanged)));
}
MinHintDisplay::MinHintDisplay(event_loop::Loop& loop,
    std::function<double()> nowMs,std::function<void()> onChanged):
    loop_(loop),nowMs_(std::move(nowMs)),onChanged_(std::move(onChanged)){}
MinHintDisplay::~MinHintDisplay(){timer_.cancel();}
MinHintDisplay::Value MinHintDisplay::render(const Value& incoming,int){
  if(!loop_.isOwnerThread())throw std::logic_error("Hint state requires loop owner");
  if(!initialized_){displayed_=incoming;initialized_=true;}
  return displayed_;
}
void MinHintDisplay::committed(const Value& incoming,int minMs){
  if(!loop_.isOwnerThread())throw std::logic_error("Hint effect requires loop owner");
  if(!initialized_)render(incoming,minMs);
  if(effectInitialized_&&effectValue_==incoming&&effectMinMs_==minMs)return;
  effectInitialized_=true;effectValue_=incoming;effectMinMs_=minMs;
  if(timer_.valid())timer_.cancel();
  const double elapsed=nowMs_()-lastShownAt_;
  if(elapsed>=minMs){
    lastShownAt_=nowMs_();
    if(displayed_!=incoming){displayed_=incoming;onChanged_();}
    return;
  }
  std::weak_ptr<MinHintDisplay> weak=shared_from_this();
  timer_=loop_.setTimeout(minMs-elapsed,[weak,incoming]{
    if(auto self=weak.lock()){
      self->lastShownAt_=self->nowMs_();
      if(self->displayed_!=incoming){self->displayed_=incoming;self->onChanged_();}
    }
  });
}
void MinHintDisplay::cancel(){
  if(!loop_.isOwnerThread())throw std::logic_error("Hint cancellation requires loop owner");
  if(timer_.valid())timer_.cancel();
  effectInitialized_=false;
}
}
