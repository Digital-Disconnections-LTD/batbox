#pragma once
#include "event_loop/Loop.hpp"
#include "event_loop/AsyncContext.hpp"
#include <atomic>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace batbox::event_loop {
using Unit=std::monostate;

// An asynchronous settlement boundary. Observer callbacks are always
// queued as owner-thread microtasks, including late `then` registrations.
template<class T>class Deferred {
public:
  using Observer=std::function<void(const T*,const std::exception_ptr&)>;
  explicit Deferred(Loop::Dispatcher dispatcher):state_(std::make_shared<State>(std::move(dispatcher))){
    if(!state_->dispatcher.valid())throw std::invalid_argument("live event loop required");
  }
  explicit Deferred(Loop& loop):Deferred(loop.dispatcher()){}
  Loop::Dispatcher dispatcher()const{return state_->dispatcher;}
  bool settled()const{return state_->settled.load(std::memory_order_acquire);}
  bool samePromise(const Deferred& other)const{return state_==other.state_;}
  void resolve(T value)const{
    auto state=state_;
    if(state->dispatcher.isOwnerThread())settle(state,std::make_shared<T>(std::move(value)),{});
    else state->dispatcher.postMission([state,value=std::make_shared<T>(std::move(value))]{
      settle(state,value,{});});
  }
  void reject(std::exception_ptr error)const{
    if(!error)throw std::invalid_argument("rejection error required");
    auto state=state_;
    if(state->dispatcher.isOwnerThread())settle(state,{},std::move(error));
    else state->dispatcher.postMission([state,error]{settle(state,{},error);});
  }
  void then(Observer observer)const{
    if(!state_->dispatcher.isOwnerThread())
      throw std::logic_error("Promise observer registration requires event loop owner");
    if(!observer)throw std::invalid_argument("Promise observer required");
    observer=runtime::bindAsyncContext(std::move(observer));
    if(!state_->settled){state_->observers.push_back(std::move(observer));return;}
    queueObserver(state_,std::move(observer));
  }
private:
  struct State {
    explicit State(Loop::Dispatcher value):dispatcher(std::move(value)){}
    Loop::Dispatcher dispatcher;
    std::atomic_bool settled=false;
    std::shared_ptr<T> value;
    std::exception_ptr error;
    std::vector<Observer> observers;
  };
  std::shared_ptr<State> state_;
  static void queueObserver(const std::shared_ptr<State>& state,Observer observer){
    state->dispatcher.queueMicrotask([state,observer=std::move(observer)]{
      observer(state->value.get(),state->error);});
  }
  static void settle(const std::shared_ptr<State>& state,std::shared_ptr<T> value,
    std::exception_ptr error){
    if(!state->dispatcher.isOwnerThread())throw std::logic_error("Promise settled off owner thread");
    if(state->settled)return;
    state->value=std::move(value);state->error=std::move(error);
    state->settled.store(true,std::memory_order_release);
    auto observers=std::move(state->observers);
    for(auto& observer:observers)queueObserver(state,std::move(observer));
  }
};

template<class T>Deferred<T> race(const Loop::Dispatcher& dispatcher,
  const std::vector<Deferred<T>>& inputs){
  if(!dispatcher.isOwnerThread())throw std::logic_error("Promise.race requires event loop owner");
  Deferred<T> output(dispatcher);
  for(const auto& input:inputs){
    if(!dispatcher.sameLoop(input.dispatcher()))
      throw std::invalid_argument("Promise.race inputs must share one event loop");
    input.then([output](const T* value,const std::exception_ptr& error){
      if(error)output.reject(error);
      else output.resolve(*value);
    });
  }
  return output;
}
template<class T>Deferred<T> race(Loop& loop,const std::vector<Deferred<T>>& inputs){
  return race<T>(loop.dispatcher(),inputs);
}

// Same-realm `async function f() { return promise; }`: adoption starts in a
// microtask, then reacts to the returned Promise. `return await promise`
// instead registers its await reaction immediately and must not use this.
template<class T>Deferred<T> adoptAsyncReturn(const Deferred<T>& inner){
  const auto dispatcher=inner.dispatcher();
  if(!dispatcher.isOwnerThread())
    throw std::logic_error("async return adoption requires event loop owner");
  Deferred<T> outer(dispatcher);
  dispatcher.queueMicrotask([inner,outer]{
    inner.then([outer](const T* value,const std::exception_ptr& error){
      if(error)outer.reject(error);else outer.resolve(*value);
    });
  });
  return outer;
}
} // namespace batbox::event_loop
