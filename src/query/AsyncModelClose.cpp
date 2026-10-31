#include "query/AsyncModelInternal.hpp"

namespace batbox::query {
event_loop::Deferred<event_loop::Unit> AsyncModelPassIterator::close(){
  return run_->close();
}
event_loop::Deferred<AsyncModelStep> AsyncModelPassIterator::throwEarly(
    std::exception_ptr error){return run_->throwEarly(std::move(error));}
} // namespace batbox::query

namespace batbox::query::async_model {
event_loop::Deferred<event_loop::Unit> Run::close(){
  if(!loop.isOwnerThread())throw std::logic_error("Async model close requires owner loop");
  if(closing)return *closing;
  if(!requests.empty())
    throw std::logic_error("Async model close queued before pending next settled");
  closing.emplace(loop);
  if(stage==Stage::Done||!cursor.close){
    cursor={};stage=Stage::Done;
    closing->resolve(event_loop::Unit{});
    return *closing;
  }
  try{
    auto pending=cursor.close();
    if(!loop.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Async model close crossed event loops");
    stage=Stage::Done;
    auto self=shared_from_this();
    pending.then([self](const event_loop::Unit*,
        const std::exception_ptr& error){
      self->cursor={};
      if(error)self->closing->reject(error);
      else self->closing->resolve(event_loop::Unit{});
    });
  }catch(...){
    cursor={};stage=Stage::Done;
    closing->reject(std::current_exception());
  }
  return *closing;
}
event_loop::Deferred<AsyncModelStep> Run::throwEarly(std::exception_ptr error){
  if(!loop.isOwnerThread())throw std::logic_error("Async model throw requires owner loop");
  if(!error)throw std::invalid_argument("Async model throw needs an exception");
  if(!requests.empty())
    throw std::logic_error("Async model throw queued before pending next settled");
  event_loop::Deferred<AsyncModelStep> response(loop);
  requests.push_back(response);
  if(stage==Stage::Done){fail(error);return response;}
  stage=Stage::InjectAwait;
  auto self=shared_from_this();
  auto resume=[self,error](const std::exception_ptr& cleanupError){
    self->cursor={};
    try{
      self->onStreamError(cleanupError?cleanupError:error);
      self->pump();
    }catch(...){self->fail(std::current_exception());}
  };
  if(!cursor.close){resume({});return response;}
  try{
    auto pending=cursor.close();
    if(!loop.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Async model throw close crossed event loops");
    pending.then([resume](const event_loop::Unit*,
        const std::exception_ptr& cleanupError){resume(cleanupError);});
  }catch(...){resume(std::current_exception());}
  return response;
}
} // namespace batbox::query::async_model
