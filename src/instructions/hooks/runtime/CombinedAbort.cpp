#include "instructions/hooks/runtime/CombinedAbort.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
std::shared_ptr<CombinedAbort> CombinedAbort::create(event_loop::Loop& loop,
  std::optional<std::stop_token> signal,
  std::optional<std::stop_token> signalB,
  std::optional<double> timeoutMs){
  if(!loop.isOwnerThread())throw std::logic_error("Combined abort owner required");
  auto state=std::shared_ptr<CombinedAbort>(new CombinedAbort());
  state->dispatcher_=loop.dispatcher();
  if((signal&&signal->stop_requested())||
    (signalB&&signalB->stop_requested())){
    state->source_.request_stop();return state;
  }
  const std::weak_ptr<CombinedAbort> weak=state;
  if(timeoutMs){
    state->timer_=loop.setTimeout(*timeoutMs,[weak]{
      if(auto current=weak.lock()){
        if(current->timer_)current->timer_->cancel();
        current->source_.request_stop();
      }
    },false);
  }
  auto abort=[weak]{if(auto current=weak.lock())current->abortFromParent();};
  if(signal)state->parentA_=std::make_unique<
    std::stop_callback<std::function<void()>>>(*signal,abort);
  if(signalB)state->parentB_=std::make_unique<
    std::stop_callback<std::function<void()>>>(*signalB,abort);
  return state;
}
void CombinedAbort::abortFromParent(){
  // std::stop_callback can run on an arbitrary cancellation thread. Abort the
  // child immediately, then cancel the libuv timer on its owner strand.
  source_.request_stop();
  const std::weak_ptr<CombinedAbort> weak=weak_from_this();
  try{dispatcher_.postMission([weak]{
    if(auto current=weak.lock())
      if(current->timer_)current->timer_->cancel();
  });}catch(const std::logic_error&){}
}
void CombinedAbort::cleanup(){
  if(!dispatcher_.isOwnerThread())
    throw std::logic_error("Combined abort cleanup owner required");
  if(cleaned_)return;
  cleaned_=true;
  if(timer_){timer_->cancel();timer_.reset();}
  parentA_.reset();parentB_.reset();
}
} // namespace batbox::instructions::hooks::runtime
