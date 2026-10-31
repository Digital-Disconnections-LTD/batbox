#include "interactive/submit/QueueReadiness.hpp"
#include <stdexcept>

namespace batbox::interactive::submit {
QueueReadiness::QueueReadiness(event_loop::Loop& owner,CommandQueue& queue,
    QueryGuard& guard,std::function<bool()> hasActiveLocalUi,
    ExecuteInput executeInput,std::function<void(std::exception_ptr)> onError):
    owner_(owner),queue_(queue),guard_(guard),
    hasActiveLocalUi_(std::move(hasActiveLocalUi)),
    executeInput_(std::move(executeInput)),onError_(std::move(onError)){
  if(!owner_.isOwnerThread())throw std::logic_error("Queue readiness owner required");
  if(!hasActiveLocalUi_||!executeInput_||!onError_)
    throw std::invalid_argument("Queue readiness imports required");
}
QueueReadiness::~QueueReadiness(){stop();}
void QueueReadiness::start(){
  if(!owner_.isOwnerThread())throw std::logic_error("Queue readiness owner required");
  if(started_)return;
  started_=true;
  const auto weak=weak_from_this();
  if(weak.expired())throw std::logic_error("Queue readiness requires shared lifetime");
  unsubscribeQueue_=queue_.subscribe([weak]{
    if(auto self=weak.lock())self->schedule();});
  unsubscribeGuard_=guard_.subscribe(
    std::make_shared<bootstrap::Signal<>::Listener>([weak]{
      if(auto self=weak.lock())self->schedule();}));
  schedule();
}
void QueueReadiness::stop(){
  if(!started_)return;
  started_=false;
  if(unsubscribeQueue_)unsubscribeQueue_();
  if(unsubscribeGuard_)unsubscribeGuard_();
  unsubscribeQueue_={};unsubscribeGuard_={};
}
void QueueReadiness::schedule(){
  if(!started_||scheduled_)return;
  scheduled_=true;
  const auto weak=weak_from_this();
  owner_.queueMicrotask([weak]{
    if(auto self=weak.lock()){
      self->scheduled_=false;
      self->check();
    }
  });
}
void QueueReadiness::check(){
  if(!owner_.isOwnerThread())throw std::logic_error("Queue readiness owner required");
  if(!started_||guard_.isActive()||hasActiveLocalUi_()||queue_.empty())return;
  try{(void)processQueueIfReady(queue_,executeInput_);}
  catch(...){onError_(std::current_exception());}
}
} // namespace batbox::interactive::submit
