#include "query/AsyncOperationQueue.hpp"
#include <stdexcept>

namespace batbox::query {
AsyncOperationQueue::AsyncOperationQueue(event_loop::Loop& loop,Next next,
    Return returnEarly,Throw throwEarly,bool drainImmediately):loop_(loop),next_(std::move(next)),
    return_(std::move(returnEarly)),throw_(std::move(throwEarly)),
    drainImmediately_(drainImmediately){
  if(!loop_.isOwnerThread())throw std::logic_error("Generator queue requires owner loop");
  if(!next_||!return_||!throw_)
    throw std::invalid_argument("Generator operation callbacks are required");
}
event_loop::Deferred<AsyncEngineStep> AsyncOperationQueue::next(){
  event_loop::Deferred<AsyncEngineStep> result(loop_);
  enqueue(Operation{Kind::Next,{}, {},result});return result;
}
event_loop::Deferred<AsyncEngineStep> AsyncOperationQueue::returnEarly(Json value){
  event_loop::Deferred<AsyncEngineStep> result(loop_);
  enqueue(Operation{Kind::Return,std::move(value),{},result});return result;
}
event_loop::Deferred<AsyncEngineStep> AsyncOperationQueue::throwEarly(
    std::exception_ptr error){
  if(!error)throw std::invalid_argument("Generator throw needs an exception");
  event_loop::Deferred<AsyncEngineStep> result(loop_);
  enqueue(Operation{Kind::Throw,{},std::move(error),result});return result;
}
void AsyncOperationQueue::enqueue(Operation operation){
  if(!loop_.isOwnerThread())
    throw std::logic_error("Generator operation requires owner loop");
  pending_.push_back(std::move(operation));pump();
}
void AsyncOperationQueue::pump(){
  if(active_||pending_.empty())return;
  const auto kind=pending_.front().kind;
  if(closed_){
    auto operation=std::move(pending_.front());pending_.pop_front();
    if(kind==Kind::Throw)operation.result.reject(operation.error);
    else operation.result.resolve(AsyncEngineStep{true,{},
      kind==Kind::Return?std::optional<Json>(operation.value):std::nullopt});
    advance();
    return;
  }
  active_=true;
  try{
    event_loop::Deferred<AsyncEngineStep> produced=
      kind==Kind::Next?next_():kind==Kind::Return?
        return_(pending_.front().value):throw_(pending_.front().error);
    if(!loop_.dispatcher().sameLoop(produced.dispatcher()))
      throw std::invalid_argument("Generator callback returned another loop's Promise");
    auto self=shared_from_this();
    produced.then([self](const AsyncEngineStep* value,
        const std::exception_ptr& error){self->settle(value,error);});
  }catch(...){settle(nullptr,std::current_exception());}
}
void AsyncOperationQueue::settle(const AsyncEngineStep* value,
    const std::exception_ptr& error){
  if(!active_||pending_.empty())
    throw std::logic_error("Generator operation settled without request");
  auto operation=std::move(pending_.front());pending_.pop_front();
  active_=false;
  if(error){
    closed_=true;
    operation.result.reject(error);
  }else{
    if(!value)throw std::logic_error("Generator operation lost a value");
    if(value->done)closed_=true;
    operation.result.resolve(*value);
  }
  advance();
}
void AsyncOperationQueue::advance(){
  if(pending_.empty())return;
  if(drainImmediately_){pump();return;}
  auto self=shared_from_this();
  loop_.queueMicrotask([self]{self->pump();});
}
} // namespace batbox::query
