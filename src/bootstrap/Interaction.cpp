#include "bootstrap/Interaction.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::bootstrap {
namespace {
constexpr double scrollDrainIdleMs=150;
void flushInner(const std::shared_ptr<InteractionState>& state,
    const InteractionServices& services){
  if(!state||!services.nowMs)
    throw std::invalid_argument("Missing interaction clock binding");
  state->lastInteractionTime=services.nowMs();
  state->interactionTimeDirty=false;
}
struct IdleWaiter:std::enable_shared_from_this<IdleWaiter> {
  std::weak_ptr<InteractionState> state;
  InteractionServices services;
  event_loop::Deferred<event_loop::Unit> output;
  explicit IdleWaiter(event_loop::Loop& loop,
      const std::shared_ptr<InteractionState>& value,
      InteractionServices imports):state(value),services(std::move(imports)),
      output(loop){}
  void poll(){
    const auto current=state.lock();
    if(!current){output.reject(std::make_exception_ptr(
      std::runtime_error("Interaction state expired")));return;}
    if(!current->scrollDraining){output.resolve({});return;}
    try{
      const auto self=shared_from_this();
      auto timer=services.setTimeout(scrollDrainIdleMs,[self]{self->poll();});
      if(timer.unref)timer.unref();
    }catch(...){output.reject(std::current_exception());}
  }
};
}
std::shared_ptr<InteractionState> makeInteractionState(
    const InteractionServices& services){
  if(!services.nowMs)
    throw std::invalid_argument("Missing interaction clock binding");
  auto state=std::make_shared<InteractionState>();
  state->lastInteractionTime=services.nowMs();
  return state;
}
void updateLastInteractionTime(const std::shared_ptr<InteractionState>& state,
    const InteractionServices& services,bool immediate){
  if(!state)throw std::invalid_argument("Missing interaction state");
  if(immediate)flushInner(state,services);
  else state->interactionTimeDirty=true;
}
void flushInteractionTime(const std::shared_ptr<InteractionState>& state,
    const InteractionServices& services){
  if(!state)throw std::invalid_argument("Missing interaction state");
  if(state->interactionTimeDirty)flushInner(state,services);
}
double getLastInteractionTime(const std::shared_ptr<InteractionState>& state){
  if(!state)throw std::invalid_argument("Missing interaction state");
  return state->lastInteractionTime;
}
void markScrollActivity(const std::shared_ptr<InteractionState>& state,
    const InteractionServices& services){
  if(!state||!services.setTimeout)
    throw std::invalid_argument("Missing scroll timer binding");
  state->scrollDraining=true;
  if(state->scrollDrainTimer&&state->scrollDrainTimer->cancel)
    state->scrollDrainTimer->cancel();
  const std::weak_ptr<InteractionState> weak=state;
  state->scrollDrainTimer=services.setTimeout(scrollDrainIdleMs,[weak]{
    if(const auto current=weak.lock()){
      current->scrollDraining=false;
      current->scrollDrainTimer.reset();
    }
  });
  if(state->scrollDrainTimer->unref)
    state->scrollDrainTimer->unref();
}
bool getIsScrollDraining(const std::shared_ptr<InteractionState>& state){
  if(!state)throw std::invalid_argument("Missing interaction state");
  return state->scrollDraining;
}
event_loop::Deferred<event_loop::Unit> waitForScrollIdle(event_loop::Loop& loop,
    const std::shared_ptr<InteractionState>& state,
    const InteractionServices& services){
  if(!state||!services.setTimeout)
    throw std::invalid_argument("Missing scroll timer binding");
  auto waiter=std::make_shared<IdleWaiter>(loop,state,services);
  waiter->poll();
  return waiter->output;
}
}
