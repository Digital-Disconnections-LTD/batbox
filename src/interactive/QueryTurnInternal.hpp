#pragma once
#include "interactive/QueryTurn.hpp"
#include <stdexcept>

namespace batbox::interactive::query_turn {
template<class F>void need(const F& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Query turn import required: ")+name);
}
struct Run:std::enable_shared_from_this<Run> {
  event_loop::Loop& owner;
  std::shared_ptr<QueryTurnState> state;
  TurnRequest request;
  std::shared_ptr<QueryTurnServices> services;
  event_loop::Deferred<event_loop::Unit> result;
  std::optional<std::uint64_t> generation;
  TurnMessagesRef latestMessages;
  std::exception_ptr bodyError;
  Run(event_loop::Loop& loop,std::shared_ptr<QueryTurnState> s,TurnRequest r,
      std::shared_ptr<QueryTurnServices> imports)
    :owner(loop),state(std::move(s)),request(std::move(r)),services(std::move(imports)),result(loop){}
  void start();
  void prepare();
  void beforeQuery();
  void invokeQuery();
  void finalize(std::exception_ptr error={});
  void finishCurrent();
  void afterTurnComplete();
  void maybeRestore();
  void settle();
  template<class T,class F>void await(event_loop::Deferred<T> pending,F continuation,
      bool inFinally=false){
    if(!owner.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Query turn awaited another owner loop");
    auto self=shared_from_this();
    pending.then([self,continuation=std::move(continuation),inFinally](
        const T* value,const std::exception_ptr& error){
      try{
        if(error)std::rethrow_exception(error);
        if(!value)throw std::logic_error("Query turn Promise has no value");
        continuation(*value);
      }catch(...){
        if(inFinally)self->result.reject(std::current_exception());
        else self->finalize(std::current_exception());
      }
    });
  }
};
} // namespace batbox::interactive::query_turn
