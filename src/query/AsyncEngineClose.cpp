#include "query/AsyncEngineInternal.hpp"
#include "query/AsyncOperationQueue.hpp"

namespace batbox::query {
void AsyncEngineIterator::ensureOperations(){
  if(operations_)return;
  if(!run_)throw std::logic_error("Moved async query iterator");
  auto run=run_;
  operations_=std::make_shared<AsyncOperationQueue>(run_->loop,
    [run]{return run->next();},
    [run](const Json& value){return run->returnEarly(value);},
    [run](std::exception_ptr error){return run->throwEarly(error);});
}
event_loop::Deferred<AsyncEngineStep> AsyncEngineIterator::next(){
  ensureOperations();return operations_->next();
}
event_loop::Deferred<AsyncEngineStep> AsyncEngineIterator::returnEarly(Json value){
  ensureOperations();return operations_->returnEarly(std::move(value));
}
event_loop::Deferred<AsyncEngineStep> AsyncEngineIterator::throwEarly(
    std::exception_ptr error){
  ensureOperations();return operations_->throwEarly(std::move(error));
}
} // namespace batbox::query

namespace batbox::query::async_engine {
event_loop::Deferred<AsyncEngineStep> Run::returnEarly(const Json& value){
  if(!loop.isOwnerThread())throw std::logic_error("Query return requires owner loop");
  if(closing)return *closing;
  if(!requests.empty())
    throw std::logic_error("Query return queued before pending next settled");
  closing.emplace(loop);
  const Json returned=value;
  if(stage==Stage::Start||stage==Stage::Done){
    stage=Stage::Done;
    closing->resolve(AsyncEngineStep{true,{},returned});
    return *closing;
  }
  auto self=shared_from_this();
  auto finalizeClose=[self,returned](const std::exception_ptr& prior){
    self->decision.reset();self->model.reset();self->abortCursor.reset();
    self->setup.reset();self->stage=Stage::Done;
    try{self->dispose(prior);}
    catch(...){self->closing->reject(std::current_exception());return;}
    if(prior)self->closing->reject(prior);
    else self->closing->resolve(AsyncEngineStep{true,{},returned});
  };
  const bool hasNested=(decision&&decision->close)||model||
    (abortCursor&&abortCursor->close);
  if(!hasNested){finalizeClose({});return *closing;}
  stage=Stage::Done;
  // AsyncGeneratorYield first awaits a received Return value. The ensuing
  // AsyncIteratorClose enters the nested generator in another continuation.
  // Keep both boundaries when the public query delegates with yield*.
  loop.queueMicrotask([self,finalizeClose]{
    self->loop.queueMicrotask([self,finalizeClose]{
    try{
      std::optional<event_loop::Deferred<event_loop::Unit>> nested;
      if(self->decision&&self->decision->close)
        nested=self->decision->close();
      else if(self->model)nested=self->model->close();
      else if(self->abortCursor&&self->abortCursor->close)
        nested=self->abortCursor->close();
      if(!nested){finalizeClose({});return;}
      if(!self->loop.dispatcher().sameLoop(nested->dispatcher()))
        throw std::invalid_argument("Nested query close crossed event loops");
      nested->then([finalizeClose](const event_loop::Unit*,
          const std::exception_ptr& error){finalizeClose(error);});
    }catch(...){finalizeClose(std::current_exception());}
    });
  });
  return *closing;
}
event_loop::Deferred<AsyncEngineStep> Run::throwEarly(std::exception_ptr error){
  if(!error)throw std::invalid_argument("Query throw needs an exception");
  if(stage==Stage::ModelStart&&model){
    event_loop::Deferred<AsyncEngineStep> result(loop);
    try{
      auto pending=model->throwEarly(error);
      if(!loop.dispatcher().sameLoop(pending.dispatcher()))
        throw std::invalid_argument("Model throw crossed event loops");
      stage=Stage::ModelAwait;
      auto self=shared_from_this();
      pending.then([self,result](const AsyncModelStep* value,
          const std::exception_ptr& nestedError){
        if(nestedError){self->fail(nestedError);result.reject(self->failure);return;}
        try{
          if(!value->done){
            self->stage=Stage::ModelStart;
            result.resolve(AsyncEngineStep{false,value->event,{},value->tombstoneTarget});
            return;
          }
          if(!value->value)throw std::logic_error("Model throw lost result");
          self->modelResult=value->value;
          self->model.reset();
          self->stage=Stage::AfterModel;
          self->requests.push_back(result);
          self->pump();
        }catch(...){self->fail(std::current_exception());result.reject(self->failure);}
      });
    }catch(...){fail(std::current_exception());result.reject(failure);}
    return result;
  }
  event_loop::Deferred<AsyncEngineStep> result(loop);
  auto closingReturn=returnEarly(Json());
  closingReturn.then([result,error](const AsyncEngineStep*,
      const std::exception_ptr& cleanupError){
    if(cleanupError)result.reject(cleanupError);
    else result.reject(error);
  });
  return result;
}
} // namespace batbox::query::async_engine
