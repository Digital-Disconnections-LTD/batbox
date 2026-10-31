#pragma once
#include "query/IndigenousCursor.hpp"
#include <functional>
#include <memory>
#include <optional>

namespace batbox::query {
// Indigenous adapter for asynchronous iterator return() after a yielded value.
// Its caller must serialize return behind an outstanding next(), as JS does.
// close() asks the worker to stop, then asynchronously drains its completion
// so producer cleanup finishes before the enclosing generator's disposer.
template<class T>class CloseableIndigenousCursor {
  struct State:std::enable_shared_from_this<State> {
    event_loop::Loop& loop;
    std::stop_source stop;
    std::function<event_loop::Deferred<std::optional<T>>()> pull;
    std::function<bool(std::exception_ptr)> expectedStop;
    std::optional<event_loop::Deferred<event_loop::Unit>> closing;
    bool started=false;
    bool done=false;
    std::unique_ptr<std::stop_callback<std::function<void()>>> external;
    explicit State(event_loop::Loop& owner):loop(owner){}
    void drain(){
      try{
        auto pending=pull();
        if(!loop.dispatcher().sameLoop(pending.dispatcher()))
          throw std::invalid_argument("Cursor close crossed event loops");
        auto self=this->shared_from_this();
        pending.then([self](const std::optional<T>* value,
            const std::exception_ptr& error){
          if(error){
            if(self->expectedStop&&self->expectedStop(error)){
              self->done=true;self->closing->resolve(event_loop::Unit{});
            }else self->closing->reject(error);
          }else if(!value||!*value){
            self->done=true;self->closing->resolve(event_loop::Unit{});
          }else self->drain();
        });
      }catch(...){
        const auto error=std::current_exception();
        if(expectedStop&&expectedStop(error)){
          done=true;closing->resolve(event_loop::Unit{});
        }else closing->reject(error);
      }
    }
  };
public:
  CloseableIndigenousCursor()=default;
  event_loop::Deferred<std::optional<T>> next()const{
    if(!state_||!state_->pull)throw std::logic_error("Unbound closeable cursor");
    if(state_->closing)throw std::logic_error("Cursor next() after close request");
    state_->started=true;
    return state_->pull();
  }
  event_loop::Deferred<event_loop::Unit> close()const{
    if(!state_)throw std::logic_error("Unbound closeable cursor");
    if(!state_->loop.isOwnerThread())
      throw std::logic_error("Cursor close requires owner loop");
    if(state_->closing)return *state_->closing;
    state_->closing.emplace(state_->loop);
    // Closing a never-started async generator does not execute its body.
    if(state_->done||!state_->started){
      state_->done=true;state_->closing->resolve(event_loop::Unit{});
      return *state_->closing;
    }
    state_->stop.request_stop();
    state_->drain();
    return *state_->closing;
  }
  bool valid()const{return !!state_;}
  template<class U>friend CloseableIndigenousCursor<U> makeCloseableIndigenousCursor(
    event_loop::Loop&,IndigenousProducer<U>,std::stop_token,
    std::function<bool(std::exception_ptr)>);
private:
  explicit CloseableIndigenousCursor(std::shared_ptr<State> state):state_(std::move(state)){}
  std::shared_ptr<State> state_;
};

template<class T>CloseableIndigenousCursor<T> makeCloseableIndigenousCursor(
    event_loop::Loop& loop,IndigenousProducer<T> producer,
    std::stop_token externalStop={},
    std::function<bool(std::exception_ptr)> expectedStop={}){
  if(!loop.isOwnerThread())throw std::logic_error("Cursor requires owner loop");
  auto state=std::make_shared<typename CloseableIndigenousCursor<T>::State>(loop);
  state->expectedStop=std::move(expectedStop);
  state->pull=makeIndigenousPullCursor<T>(loop,std::move(producer),
    state->stop.get_token());
  if(externalStop.stop_possible()){
    std::weak_ptr<typename CloseableIndigenousCursor<T>::State> weak=state;
    state->external=std::make_unique<
      std::stop_callback<std::function<void()>>>(externalStop,
      [weak]{if(auto current=weak.lock())current->stop.request_stop();});
  }
  return CloseableIndigenousCursor<T>(std::move(state));
}
} // namespace batbox::query
