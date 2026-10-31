#include "lodash_throttle/Throttle.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace batbox::lodash_throttle {
Throttle::Throttle(std::function<void()> callback,double wait,
    Services services):callback_(std::move(callback)),
    services_(std::move(services)),
    wait_(std::isnan(wait)?0:wait),maxWait_(wait_){
  if(!callback_||!services_.now||!services_.setTimeout)
    throw std::invalid_argument("Throttle callback and timer imports required");
}
void Throttle::invokeFunc(double time){
  lastArgs_=false;
  lastInvokeTime_=time;
  callback_();
}
void Throttle::schedule(double delay){
  auto self=shared_from_this();
  timerId_=services_.setTimeout(delay,[self]{self->timerExpired();});
  if(!timerId_->cancel)throw std::invalid_argument(
    "Throttle timer cancellation required");
}
void Throttle::leadingEdge(double time){
  lastInvokeTime_=time;
  schedule(wait_);
  invokeFunc(time);
}
double Throttle::remainingWait(double time)const{
  const auto sinceLastCall=time-*lastCallTime_;
  const auto sinceLastInvoke=time-lastInvokeTime_;
  const auto waiting=wait_-sinceLastCall;
  return std::min(waiting,maxWait_-sinceLastInvoke);
}
bool Throttle::shouldInvoke(double time)const{
  if(!lastCallTime_)return true;
  const auto sinceLastCall=time-*lastCallTime_;
  const auto sinceLastInvoke=time-lastInvokeTime_;
  return sinceLastCall>=wait_||sinceLastCall<0||
    sinceLastInvoke>=maxWait_;
}
void Throttle::timerExpired(){
  const auto time=services_.now();
  if(shouldInvoke(time)){trailingEdge(time);return;}
  schedule(remainingWait(time));
}
void Throttle::trailingEdge(double time){
  timerId_.reset();
  if(lastArgs_){invokeFunc(time);return;}
  lastArgs_=false;
}
void Throttle::call(){
  const auto time=services_.now();
  const auto isInvoking=shouldInvoke(time);
  lastArgs_=true;
  lastCallTime_=time;
  if(isInvoking){
    if(!timerId_){leadingEdge(time);return;}
    // maxWait is enabled by throttle() even when a trailing timer exists.
    timerId_->cancel();
    schedule(wait_);
    invokeFunc(time);
    return;
  }
  if(!timerId_)schedule(wait_);
}
void Throttle::cancel(){
  if(timerId_)timerId_->cancel();
  lastInvokeTime_=0;
  lastArgs_=false;
  lastCallTime_.reset();
  timerId_.reset();
}
void Throttle::flush(){
  if(timerId_)trailingEdge(services_.now());
}
} // namespace batbox::lodash_throttle
