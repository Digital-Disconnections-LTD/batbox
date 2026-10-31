#include "cleanup/Roster.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace batbox::cleanup {
struct Roster::State {
  explicit State(event_loop::Loop& owner):loop(owner){}
  event_loop::Loop& loop;
  std::vector<CleanupIdentity> callbacks;
};
Roster::Roster(event_loop::Loop& loop):state_(std::make_shared<State>(loop)){}
std::function<void()> Roster::registerCleanup(CleanupIdentity fn){
  if(!state_->loop.isOwnerThread())throw std::logic_error("cleanup registry requires loop owner");
  if(!fn||!*fn)throw std::invalid_argument("cleanup function required");
  const auto found=std::find(state_->callbacks.begin(),state_->callbacks.end(),fn);
  if(found==state_->callbacks.end())state_->callbacks.push_back(fn);
  std::weak_ptr<State> weak=state_;
  return [weak,fn]{
    const auto state=weak.lock();if(!state)return;
    if(!state->loop.isOwnerThread())throw std::logic_error("cleanup unregister requires loop owner");
    const auto found=std::find(state->callbacks.begin(),state->callbacks.end(),fn);
    if(found!=state->callbacks.end())state->callbacks.erase(found);
  };
}
std::function<void()> Roster::registerCleanup(CleanupFunction fn){
  return registerCleanup(std::make_shared<CleanupFunction>(std::move(fn)));
}
std::size_t Roster::size()const{return state_->callbacks.size();}
Deferred<Unit> Roster::runCleanupFunctions(){
  auto& loop=state_->loop;
  if(!loop.isOwnerThread())throw std::logic_error("cleanup run requires loop owner");
  Deferred<Unit> result(loop);
  // Array.from(Set) snapshots insertion order. Every function is invoked in
  // that order before Promise.all attaches its reactions.
  const auto snapshot=state_->callbacks;
  std::vector<Deferred<Unit>> pending;
  pending.reserve(snapshot.size());
  try{for(const auto& fn:snapshot)pending.push_back((*fn)());}
  catch(...){result.reject(std::current_exception());return result;}
  if(pending.empty()){
    loop.queueMicrotask([result]{result.resolve(Unit{});});
    return result;
  }
  struct Aggregate {std::size_t remaining;bool done=false;};
  auto aggregate=std::make_shared<Aggregate>(Aggregate{pending.size(),false});
  for(const auto& promise:pending){
    if(!loop.dispatcher().sameLoop(promise.dispatcher()))
      throw std::invalid_argument("cleanup promises require owner loop");
    promise.then([dispatcher=loop.dispatcher(),aggregate,result](const Unit*,
      const std::exception_ptr& error){
      if(aggregate->done)return;
      if(error){aggregate->done=true;
        dispatcher.queueMicrotask([result,error]{result.reject(error);});
        return;
      }
      if(--aggregate->remaining==0){aggregate->done=true;
        dispatcher.queueMicrotask([result]{result.resolve(Unit{});});
      }
    });
  }
  return result;
}
} // namespace batbox::cleanup
