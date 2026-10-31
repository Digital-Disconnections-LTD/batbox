#include "query_engine/LifecycleInternal.hpp"

namespace batbox::query_engine {
ConversationTurn Conversation::submitMessage(event_loop::Loop& loop,Json poetry,
    SubmitOptions options,std::shared_ptr<LifecycleServices> services){
  if(!loop.isOwnerThread())throw std::logic_error("Conversation requires owner loop");
  if(!services)throw std::invalid_argument("Conversation lifecycle imports required");
  auto run=std::make_shared<lifecycle::Run>(loop,state_,std::move(poetry),
    std::move(options),std::move(services));
  return ConversationTurn(std::make_shared<query::AsyncOperationQueue>(loop,
    [run]{return run->next();},[run](const Json& value){return run->close(value);},
    [run](std::exception_ptr error){return run->close(lifecycle::undefined(),error);}));
}
}
namespace batbox::query_engine::lifecycle {
event_loop::Deferred<query::AsyncEngineStep> Run::next(){
  request.emplace(loop);auto output=*request;yielded=false;pump();return output;
}
void Run::emit(Json value,Stage after){
  stage=after;yielded=true;
  auto output=*request;request.reset();
  output.resolve(query::AsyncEngineStep{false,std::make_shared<Json>(std::move(value)),{}});
}
void Run::finish(){
  stage=Stage::Done;auto output=*request;request.reset();
  output.resolve(query::AsyncEngineStep{true,{},{}});
}
event_loop::Deferred<query::AsyncEngineStep> Run::close(Json value,std::exception_ptr error){
  closing.emplace(loop);auto output=*closing;stage=Stage::Done;
  auto settle=[output,value=std::move(value),error](std::exception_ptr closeError){
    // AsyncIteratorClose preserves a pending throw over a close rejection.
    if(error)output.reject(error);
    else if(closeError)output.reject(closeError);
    else output.resolve(query::AsyncEngineStep{true,{},value});
  };
  try{
    auto* cursor=orphanActive?&orphanCursor:queryActive?&queryCursor:nullptr;
    orphanActive=false;queryActive=false;
    if(!cursor){settle({});return output;}
    auto pending=cursor->close();
    if(!loop.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Conversation close crossed event loops");
    pending.then([settle](const event_loop::Unit*,const std::exception_ptr& failure){settle(failure);});
  }catch(...){settle(std::current_exception());}
  return output;
}
void Run::fail(std::exception_ptr error){
  if(!request)return;
  auto output=*request;request.reset();
  // Rejection from iterator.next() does not invoke AsyncIteratorClose.
  if(!inBody){queryActive=false;orphanActive=false;}
  auto pending=close(undefined(),error);
  pending.then([output](const query::AsyncEngineStep*,const std::exception_ptr& failure){
    output.reject(failure);
  });
}
void Run::pump(){
  try{
    while(request&&!waiting&&!yielded){
      switch(stage){
        case Stage::Start:start();break;
        case Stage::PrepareContext:prepareContext();break;
        case Stage::OrphanNext:pullOrphan();break;
        case Stage::UserInput:startInput();break;
        case Stage::InputAfterRecord:afterInputRecord();break;
        case Stage::InputAfterFlush:afterInputFlush();break;
        case Stage::InitResume:afterInit();break;
        case Stage::LocalMessages:localMessages();break;
        case Stage::LocalPersist:localPersist();break;
        case Stage::LocalResult:localResult();break;
        case Stage::QueryStart:startQuery();break;
        case Stage::QueryNext:pullQuery();break;
        case Stage::QueryBeforeRecord:beforeRecord();break;
        case Stage::QueryAfterRecord:acknowledge();break;
        case Stage::Ack:acknowledge();break;
        case Stage::Dispatch:dispatch();break;
        case Stage::NormalizedNext:normalizedNext();break;
        case Stage::SystemAfterCompact:systemAfterCompact();break;
        case Stage::PostSwitch:postSwitch();break;
        case Stage::RetryCheck:retryCheck();break;
        case Stage::FinishQuery:finishQuery();break;
        case Stage::FinalResult:finalResult();break;
        case Stage::Done:
          if(terminalAfterEmit&&queryActive){
            terminalAfterEmit=false;queryActive=false;
            await(queryCursor.close(),[this](const event_loop::Unit&){finish();});
          }else finish();
          break;
      }
    }
  }catch(...){fail(std::current_exception());}
}
}
