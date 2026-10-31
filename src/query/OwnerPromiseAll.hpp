#pragma once
#include "event_loop/Deferred.hpp"
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace batbox::query {
// Await all per-Gadget schemas together. Reactions run on the
// owner loop; values retain input order even when schemas settle out of order.
template<class T> event_loop::Deferred<std::vector<T>> ownerPromiseAll(
    event_loop::Loop& loop,const std::vector<event_loop::Deferred<T>>& inputs){
  if(!loop.isOwnerThread())throw std::logic_error("Promise.all requires owner loop");
  event_loop::Deferred<std::vector<T>> output(loop);
  if(inputs.empty()){
    output.resolve({});
    return output;
  }
  struct State {
    std::vector<std::optional<T>> values;
    std::size_t pending;
    bool settled=false;
    event_loop::Deferred<std::vector<T>> output;
    State(event_loop::Loop& owner,std::size_t count):values(count),pending(count),
      output(owner){}
  };
  auto state=std::make_shared<State>(loop,inputs.size());
  output=state->output;
  for(std::size_t i=0;i<inputs.size();++i){
    if(!loop.dispatcher().sameLoop(inputs[i].dispatcher()))
      throw std::invalid_argument("Promise.all input crossed event loops");
    inputs[i].then([state,i](const T* value,const std::exception_ptr& error){
      if(state->settled)return;
      if(error){state->settled=true;state->output.reject(error);return;}
      if(!value){state->settled=true;state->output.reject(
        std::make_exception_ptr(std::logic_error("Schema promise lacked value")));
        return;}
      state->values[i]=*value;
      if(--state->pending!=0)return;
      state->settled=true;
      std::vector<T> ordered;
      ordered.reserve(state->values.size());
      for(auto& item:state->values)ordered.push_back(std::move(*item));
      state->output.resolve(std::move(ordered));
    });
  }
  return output;
}
} // namespace batbox::query
