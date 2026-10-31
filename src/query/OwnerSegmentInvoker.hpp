#pragma once
#include "event_loop/Deferred.hpp"
#include "transport/Errors.hpp"
#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <type_traits>
#include <utility>

namespace batbox::query {
// Indigenous adapter for one complete synchronous segment between awaits.
// The worker waits; all imported callbacks inside the segment run together on
// the owner. If cancellation wins before the owner starts it, the mission skips
// the closure so worker-local references cannot be used after return.
template<class F> auto invokeOwnerSegment(
    const event_loop::Loop::Dispatcher& owner,std::stop_token stop,F action)
    -> std::invoke_result_t<F> {
  using Result=std::invoke_result_t<F>;
  static_assert(!std::is_void_v<Result>);
  if(owner.isOwnerThread())return action();
  if(!owner.valid())throw std::logic_error("Owner loop has expired");
  struct State {
    std::mutex mutex;
    std::condition_variable changed;
    bool running=false;
    bool finished=false;
    bool skipped=false;
    std::optional<Result> value;
    std::exception_ptr failure;
  };
  auto state=std::make_shared<State>();
  std::stop_callback onStop(stop,[state]{
    std::lock_guard lock(state->mutex);
    if(state->finished||state->running)return;
    state->skipped=true;state->finished=true;state->changed.notify_all();
  });
  if(!stop.stop_requested())owner.postMission([state,action=std::move(action)]() mutable {
    {
      std::lock_guard lock(state->mutex);
      if(state->skipped)return;
      state->running=true;
    }
    try{
      Result value=action();
      std::lock_guard lock(state->mutex);
      state->value=std::move(value);
      state->finished=true;
    }catch(...){
      std::lock_guard lock(state->mutex);
      state->failure=std::current_exception();
      state->finished=true;
    }
    state->changed.notify_all();
  });
  std::unique_lock lock(state->mutex);
  state->changed.wait(lock,[&]{return state->finished;});
  if(state->skipped)throw transport::APIUserAbortError();
  if(state->failure)std::rethrow_exception(state->failure);
  if(!state->value)throw std::logic_error("Owner segment finished without value");
  return std::move(*state->value);
}

template<class T>T waitOwnerDeferred(
    const event_loop::Loop::Dispatcher& owner,std::stop_token stop,
    event_loop::Deferred<T> pending){
  if(owner.isOwnerThread())throw std::logic_error(
    "Awaited owner import cannot block the owner loop");
  if(!owner.sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Awaited import crossed event loops");
  struct State {
    std::mutex mutex;
    std::condition_variable changed;
    bool finished=false;
    bool cancelled=false;
    std::optional<T> value;
    std::exception_ptr failure;
  };
  auto state=std::make_shared<State>();
  std::stop_callback onStop(stop,[state]{
    std::lock_guard lock(state->mutex);
    if(state->finished)return;
    state->cancelled=true;state->finished=true;state->changed.notify_all();
  });
  owner.postMission([state,pending]{
    {
      std::lock_guard lock(state->mutex);
      if(state->cancelled)return;
    }
    pending.then([state](const T* value,const std::exception_ptr& failure){
      std::lock_guard lock(state->mutex);
      if(state->finished)return;
      if(value)state->value=*value;
      state->failure=failure;
      state->finished=true;
      state->changed.notify_all();
    });
  });
  std::unique_lock lock(state->mutex);
  state->changed.wait(lock,[&]{return state->finished;});
  if(state->cancelled)throw transport::APIUserAbortError();
  if(state->failure)std::rethrow_exception(state->failure);
  if(!state->value)throw std::logic_error("Awaited owner import settled without value");
  return std::move(*state->value);
}
} // namespace batbox::query
