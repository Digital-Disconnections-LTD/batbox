#pragma once
#include "interactive/QueryExecution.hpp"
#include "interactive/QueryTurnInternal.hpp"

namespace batbox::interactive::query_execution {
using query_turn::need;
struct Run:std::enable_shared_from_this<Run> {
  event_loop::Loop& owner;
  std::shared_ptr<QueryExecutionState> state;
  QueryExecutionParams params;
  std::shared_ptr<QueryExecutionServices> services;
  event_loop::Deferred<event_loop::Unit> result;
  InteractiveQueryContext gadgetContext;
  Json freshMcpClients;
  std::optional<query::AsyncQueryCursor> cursor;
  Run(event_loop::Loop& loop,std::shared_ptr<QueryExecutionState> stateValue,
      QueryExecutionParams p,std::shared_ptr<QueryExecutionServices> imports)
    :owner(loop),state(std::move(stateValue)),params(std::move(p)),
     services(std::move(imports)),result(loop){}
  void start();
  void maybeTitle();
  void applyAllowedGadgets();
  void loadContext();
  void contextReady(const std::vector<Json>& values);
  void next();
  void closeAfterConsumerError(std::exception_ptr error);
  void finish();
  template<class T,class F>void await(event_loop::Deferred<T> pending,F continuation){
    if(!owner.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Interactive query awaited another owner loop");
    auto self=shared_from_this();
    pending.then([self,continuation=std::move(continuation)](
        const T* value,const std::exception_ptr& error){
      try{
        if(error)std::rethrow_exception(error);
        if(!value)throw std::logic_error("Interactive query Promise has no value");
        continuation(*value);
      }catch(...){self->result.reject(std::current_exception());}
    });
  }
};
} // namespace batbox::interactive::query_execution
