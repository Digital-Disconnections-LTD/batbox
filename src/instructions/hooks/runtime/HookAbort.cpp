#include "instructions/hooks/runtime/HookAbort.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
std::shared_ptr<HookAbort> HookAbort::create(event_loop::Loop& loop,
  std::stop_token parent,double timeoutMs){
  if(!loop.isOwnerThread())throw std::logic_error("Hook timeout owner required");
  auto state=std::shared_ptr<HookAbort>(new HookAbort());
  const std::weak_ptr<HookAbort> weak=state;
  state->parentCallback_=std::make_unique<
    std::stop_callback<std::function<void()>>>(parent,[weak]{
      if(auto current=weak.lock())current->source_.request_stop();
    });
  state->timer_=loop.setTimeout(timeoutMs,[weak]{
    if(auto current=weak.lock())current->source_.request_stop();
  });
  return state;
}
void HookAbort::cleanup(){
  if(timer_){timer_->cancel();timer_.reset();}
  parentCallback_.reset();
}
} // namespace batbox::instructions::hooks::runtime
