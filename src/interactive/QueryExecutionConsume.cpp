#include "interactive/QueryExecutionInternal.hpp"

namespace batbox::interactive::query_execution {
void Run::next(){
  need(cursor->next,"query.next");auto self=shared_from_this();
  await(cursor->next(),[self](const query::AsyncEngineStep& step){
    if(step.done){self->finish();return;}
    try{
      need(self->services->onQueryEvent,"onQueryEvent");
      self->services->onQueryEvent(step);
    }catch(...){self->closeAfterConsumerError(std::current_exception());return;}
    self->next();
  });
}
void Run::closeAfterConsumerError(std::exception_ptr error){
  // AsyncIteratorClose preserves the consumer's abrupt completion, including
  // when return() itself rejects. Failure from next() does not call return().
  if(!cursor->returnEarly){result.reject(error);return;}
  try{
    auto closed=cursor->returnEarly(Json(Json::value_t::discarded));
    if(!owner.dispatcher().sameLoop(closed.dispatcher()))
      throw std::invalid_argument("Query close awaited another owner loop");
    const auto done=result;
    closed.then([done,error](const query::AsyncEngineStep*,const std::exception_ptr&){
      done.reject(error);
    });
  }catch(...){result.reject(error);}
}
void Run::finish(){
  auto& s=*services;
  if(s.feature("MONSTER")){
    need(s.getMessages,"getMessages");need(s.launchMonsterObserver,"launchMonsterObserver");
    s.launchMonsterObserver(s.getMessages());
  }
  s.checkpoint("query_end");
  need(s.resetLoadingState,"resetLoadingState");s.resetLoadingState();
  need(s.logQueryProfileReport,"logQueryProfileReport");s.logQueryProfileReport();
  if(s.onTurnComplete){
    need(s.getMessages,"getMessages");auto self=shared_from_this();
    await(s.onTurnComplete(s.getMessages()),[self](const event_loop::Unit&){
      self->result.resolve(event_loop::Unit{});
    });
  }else{
    auto done=result;owner.dispatcher().queueMicrotask([done]{done.resolve(event_loop::Unit{});});
  }
}
} // namespace batbox::interactive::query_execution
namespace batbox::interactive {
void bindQueryExecutionEngine(event_loop::Loop& owner,QueryExecutionServices& services,
    std::shared_ptr<query::AsyncEngineServices> engine,query::NotifyLifecycle notify){
  if(!engine)throw std::invalid_argument("Interactive query engine services required");
  services.makeQuery=[&owner,engine=std::move(engine),notify=std::move(notify)](query::Params params){
    auto iterator=std::make_shared<query::AsyncQueryIterator>(
      query::makeAsyncQuery(owner,std::move(params),*engine,notify));
    return query::AsyncQueryCursor{
      [iterator]{return iterator->next();},
      [iterator](const Json& value){return iterator->returnEarly(value);},
      [iterator](std::exception_ptr error){return iterator->throwEarly(std::move(error));}};
  };
}
} // namespace batbox::interactive
