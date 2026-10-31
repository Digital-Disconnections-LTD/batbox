#include "query/AsyncEntry.hpp"
#include "query/AsyncOperationQueue.hpp"
#include <stdexcept>

namespace batbox::query {
namespace {
enum class Kind {Next,Return,Throw};
struct Run:std::enable_shared_from_this<Run> {
  event_loop::Loop& loop;
  Params params;
  AsyncQueryImports imports;
  std::vector<std::string> consumedCommandUuids;
  std::optional<AsyncQueryCursor> child;

  Run(event_loop::Loop& owner,Params p,AsyncQueryImports bindings)
    :loop(owner),params(std::move(p)),imports(std::move(bindings)){}

  event_loop::Deferred<AsyncEngineStep> relay(Kind kind,Json returned={},
      std::exception_ptr thrown={}){
    event_loop::Deferred<AsyncEngineStep> result(loop);
    try{
      // An async generator has not entered its body before its first next().
      if(!child){
        if(kind==Kind::Return){
          result.resolve(AsyncEngineStep{true,{},std::move(returned)});
          return result;
        }
        if(kind==Kind::Throw){result.reject(thrown);return result;}
        if(!imports.queryLoop)throw std::invalid_argument("Unbound async queryLoop");
        child=imports.queryLoop(std::move(params),consumedCommandUuids);
      }
      event_loop::Deferred<AsyncEngineStep> pending=invoke(kind,returned,thrown);
      if(!loop.dispatcher().sameLoop(pending.dispatcher()))
        throw std::invalid_argument("Delegated query belongs to another event loop");
      auto self=shared_from_this();
      pending.then([self,result,kind](const AsyncEngineStep* value,
          const std::exception_ptr& error){
        try{
          if(error){self->child.reset();result.reject(error);return;}
          if(!value)throw std::logic_error("Delegated query lost its result");
          if(value->done){
            // `yield*` skips the following statements for a Return completion.
            // A handled throw can complete normally and reaches the statements.
            if(kind!=Kind::Return){
              for(std::size_t index=0;index<self->consumedCommandUuids.size();++index){
                const auto uuid=self->consumedCommandUuids[index];
                if(!self->imports.notifyCommandLifecycle)
                  throw std::invalid_argument("Unbound notifyCommandLifecycle");
                self->imports.notifyCommandLifecycle(uuid,"completed");
              }
            }
            self->child.reset();
            self->consumedCommandUuids.clear();
          }
          result.resolve(*value);
        }catch(...){
          self->child.reset();result.reject(std::current_exception());
        }
      });
    }catch(...){child.reset();result.reject(std::current_exception());}
    return result;
  }

  event_loop::Deferred<AsyncEngineStep> invoke(Kind kind,const Json& returned,
      const std::exception_ptr& thrown){
    if(kind==Kind::Next){
      if(!child->next)throw std::invalid_argument("Unbound delegated query next");
      return child->next();
    }
    if(kind==Kind::Return){
      if(!child->returnEarly)
        throw std::invalid_argument("Unbound delegated query return");
      return child->returnEarly(returned);
    }
    if(!child->throwEarly)throw std::invalid_argument("Unbound delegated query throw");
    return child->throwEarly(thrown);
  }
};
}

AsyncQueryIterator::AsyncQueryIterator(std::shared_ptr<AsyncOperationQueue> operations)
  :operations_(std::move(operations)){
  if(!operations_)throw std::invalid_argument("Async query operations are required");
}
event_loop::Deferred<AsyncEngineStep> AsyncQueryIterator::next(){
  if(!operations_)throw std::logic_error("Moved async query iterator");
  return operations_->next();
}
event_loop::Deferred<AsyncEngineStep> AsyncQueryIterator::returnEarly(Json value){
  if(!operations_)throw std::logic_error("Moved async query iterator");
  return operations_->returnEarly(std::move(value));
}
event_loop::Deferred<AsyncEngineStep> AsyncQueryIterator::throwEarly(std::exception_ptr error){
  if(!operations_)throw std::logic_error("Moved async query iterator");
  return operations_->throwEarly(std::move(error));
}
AsyncQueryIterator makeAsyncQuery(event_loop::Loop& loop,Params params,
    AsyncQueryImports imports){
  if(!loop.isOwnerThread())throw std::logic_error("Async query requires owner loop");
  auto run=std::make_shared<Run>(loop,std::move(params),std::move(imports));
  return AsyncQueryIterator(std::make_shared<AsyncOperationQueue>(loop,
    [run]{return run->relay(Kind::Next);},
    [run](const Json& value){return run->relay(Kind::Return,value);},
    [run](std::exception_ptr error){return run->relay(Kind::Throw,{},error);},true));
}
AsyncQueryIterator makeAsyncQuery(event_loop::Loop& loop,Params params,
    AsyncEngineServices services,NotifyLifecycle notifyCommandLifecycle){
  AsyncQueryImports imports;
  imports.notifyCommandLifecycle=std::move(notifyCommandLifecycle);
  imports.queryLoop=[&loop,services=std::move(services)](
      Params p,std::vector<std::string>& consumed) mutable {
    auto iterator=std::make_shared<AsyncEngineIterator>(
      makeAsyncQueryLoop(loop,std::move(p),consumed,std::move(services)));
    return AsyncQueryCursor{
      [iterator]{return iterator->next();},
      [iterator](const Json& value){return iterator->returnEarly(value);},
      [iterator](std::exception_ptr error){return iterator->throwEarly(error);}};
  };
  return makeAsyncQuery(loop,std::move(params),std::move(imports));
}
} // namespace batbox::query
