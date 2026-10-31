#include "query/AsyncCompletionInternal.hpp"
#include "query/AsyncGadgetTurnInternal.hpp"

namespace batbox::query::async_completion {
event_loop::Deferred<event_loop::Unit> Run::close(){
  if(!loop.isOwnerThread())throw std::logic_error("Completion close requires owner loop");
  if(closing)return *closing;
  if(!requests.empty())
    throw std::logic_error("Completion close queued before pending next settled");
  closing.emplace(loop);
  stage=Stage::Done;
  if(!stopCursor||!stopCursor->close){
    stopCursor.reset();closing->resolve(event_loop::Unit{});
    return *closing;
  }
  try{
    auto pending=stopCursor->close();
    if(!loop.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Stop-hook close crossed event loops");
    auto self=shared_from_this();
    pending.then([self](const event_loop::Unit*,const std::exception_ptr& error){
      self->stopCursor.reset();
      if(error)self->closing->reject(error);
      else self->closing->resolve(event_loop::Unit{});
    });
  }catch(...){
    stopCursor.reset();closing->reject(std::current_exception());
  }
  return *closing;
}
} // namespace batbox::query::async_completion

namespace batbox::query::async_gadget_turn {
event_loop::Deferred<event_loop::Unit> Run::close(){
  if(!loop.isOwnerThread())throw std::logic_error("Tool-turn close requires owner loop");
  if(closing)return *closing;
  if(!requests.empty())
    throw std::logic_error("Tool-turn close queued before pending next settled");
  closing.emplace(loop);
  stage=Stage::Done;
  auto self=shared_from_this();
  auto finish=[self](const std::exception_ptr& error){
    self->gadgetCursor={};self->attachmentCursor={};
    if(error)self->closing->reject(error);
    else self->closing->resolve(event_loop::Unit{});
  };
  auto closeAttachment=[self,finish]{
    if(!self->attachmentCursor.close){finish({});return;}
    try{
      auto pending=self->attachmentCursor.close();
      if(!self->loop.dispatcher().sameLoop(pending.dispatcher()))
        throw std::invalid_argument("Attachment close crossed event loops");
      pending.then([finish](const event_loop::Unit*,const std::exception_ptr& error){
        finish(error);
      });
    }catch(...){finish(std::current_exception());}
  };
  if(!gadgetCursor.close){closeAttachment();return *closing;}
  try{
    auto pending=gadgetCursor.close();
    if(!loop.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Tool close crossed event loops");
    pending.then([closeAttachment,finish](const event_loop::Unit*,
        const std::exception_ptr& error){
      if(error){finish(error);return;}
      closeAttachment();
    });
  }catch(...){finish(std::current_exception());}
  return *closing;
}
} // namespace batbox::query::async_gadget_turn
