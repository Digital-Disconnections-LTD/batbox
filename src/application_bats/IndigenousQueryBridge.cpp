#include "application_bats/IndigenousQueryBridge.hpp"
#include "application/Async.hpp"
#include "runtime/MessageSnapshot.hpp"
#include <stdexcept>

namespace batbox::application_bats {
IndigenousQueryBridge::IndigenousQueryBridge(event_loop::Loop& owner,
    MakeBinding makeBinding,query::NotifyLifecycle notify,
    std::function<void(const std::string&,const Json&)> observe):
    owner_(owner),makeBinding_(std::move(makeBinding)),
    notify_(std::move(notify)),observe_(std::move(observe)){
  if(!makeBinding_)throw std::invalid_argument("Subagent query engine factory required");
  if(!notify_)throw std::invalid_argument("Subagent lifecycle callback required");
}
void IndigenousQueryBridge::run(const query::Params& params,
    const bats::BatContext& context,const CanUseGadget& canUseGadget,
    const query::Yield& yield)const{
  const auto dispatcher=owner_.dispatcher();
  if(dispatcher.isOwnerThread())
    throw std::logic_error("Subagent query bridge requires a worker thread");
  struct Active {
    IndigenousQueryBinding binding;
    std::shared_ptr<query::AsyncQueryIterator> iterator;
  };
  auto active=application::onOwnerAndWait<std::shared_ptr<Active>>(
    dispatcher,[this,params,context,canUseGadget]{
      auto current=std::make_shared<Active>();
      current->binding=makeBinding_(context,canUseGadget);
      if(!current->binding.engine)
        throw std::invalid_argument("Subagent query engine is unbound");
      current->iterator=std::make_shared<query::AsyncQueryIterator>(
        query::makeAsyncQuery(owner_,params,*current->binding.engine,notify_));
      return current;
    });
  try{
    for(;;){
      auto step=application::awaitOnOwner<query::AsyncEngineStep>(dispatcher,
        [active]{return active->iterator->next();});
      if(step.done)break;
      if(step.event){
        if(observe_){
          const auto& id=value::property(context.query.data,"agentId");
          if(id.is_string())application::onOwnerAndWait<void>(dispatcher,
            [this,id=id.get<std::string>(),event=runtime::snapshotMessage(step.event)]{observe_(id,event);});
        }
        yield(step.event);
      }
    }
  }catch(...){
    const auto error=std::current_exception();
    try{(void)application::awaitOnOwner<query::AsyncEngineStep>(dispatcher,
      [active]{return active->iterator->returnEarly(Json());});}catch(...){}
    std::rethrow_exception(error);
  }
}
} // namespace batbox::application_bats
