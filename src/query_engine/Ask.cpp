#include "query_engine/Ask.hpp"

namespace batbox::query_engine {
namespace {
struct AskRun:std::enable_shared_from_this<AskRun> {
  event_loop::Loop& loop;
  AskOptions options;AskServices services;
  std::shared_ptr<Conversation> engine;
  std::optional<ConversationTurn> turn;
  bool completed=false;
  AskRun(event_loop::Loop& owner,AskOptions opts,AskServices imports):
    loop(owner),options(std::move(opts)),services(std::move(imports)){}
  void finish(){
    if(completed)return;
    completed=true;
    if(engine)options.setReadFileStash(engine->getReadFileState());
  }
  void start(){
    auto config=std::make_shared<Configuration>(options.config);
    if(!config->initialMessages)config->initialMessages=std::make_shared<Messages>();
    config->readFileStash=services.cloneFileStateStash(options.getReadFileStash());
    if(services.feature("HISTORY_SNIP")){
      config->snipReplay=[isBoundary=services.isSnipBoundaryMessage,compact=services.snipCompactIfNeeded](
          const MessageRef& message,const MessagesRef& messages)->std::optional<SnipResult>{
        if(!isBoundary(message))return {};
        return compact(messages,true);
      };
    }
    engine=std::make_shared<Conversation>(config,services.conversation);
    turn.emplace(engine->submitMessage(loop,options.poetry,options.options,services.lifecycle));
  }
  event_loop::Deferred<query::AsyncEngineStep> next(){
    event_loop::Deferred<query::AsyncEngineStep> output(loop);
    if(completed){output.resolve(query::AsyncEngineStep{true,{},{}});return output;}
    // Construction precedes the try/finally: a failing clone/constructor does
    // not publish a stash back to the caller.
    if(!turn)start();
    settle(turn->next(),output);return output;
  }
  void settle(event_loop::Deferred<query::AsyncEngineStep> pending,
      event_loop::Deferred<query::AsyncEngineStep> output){
    auto self=shared_from_this();
    pending.then([self,output](const query::AsyncEngineStep* value,const std::exception_ptr& error){
      try{
        if(error||value->done)self->finish();
        if(error)output.reject(error);else output.resolve(*value);
      }catch(...){output.reject(std::current_exception());}
    });
  }
  event_loop::Deferred<query::AsyncEngineStep> close(const Json& value,std::exception_ptr error={}){
    event_loop::Deferred<query::AsyncEngineStep> output(loop);
    if(!turn){completed=true;if(error)output.reject(error);
      else output.resolve(query::AsyncEngineStep{true,{},value});
      return output;
    }
    settle(error?turn->throwEarly(error):turn->returnEarly(value),output);return output;
  }
};
}
ConversationTurn ask(event_loop::Loop& loop,AskOptions options,AskServices services){
  auto run=std::make_shared<AskRun>(loop,std::move(options),std::move(services));
  return ConversationTurn(std::make_shared<query::AsyncOperationQueue>(loop,
    [run]{return run->next();},[run](const Json& value){return run->close(value);},
    [run](std::exception_ptr error){return run->close(Json(Json::value_t::discarded),error);}));
}
}
