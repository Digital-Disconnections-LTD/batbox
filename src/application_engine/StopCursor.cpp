#include "application_engine/Engine.hpp"
#include "query/CloseableIndigenousCursor.hpp"

namespace batbox::application_engine {
query::AsyncStopCursor makeStopCursor(event_loop::Loop& owner,
    const Json& messages,const Json& assistants,const query::Params& params,
    const execution::Context& context,std::optional<bool> active,
    query::StopHookServices services){
  auto terminal=std::make_shared<Json>();
  auto cursor=query::makeCloseableIndigenousCursor<model::MessageRef>(owner,
    [messages,assistants,params,context,active,services=std::move(services),terminal](
        const auto& yield,std::stop_token stop)mutable{
      auto current=context;current.stop=stop;
      *terminal=query::handleStopHooks(messages,assistants,params,current,active,
        services,[&](const Json& event){yield(std::make_shared<Json>(event));});
    },context.stop);
  return query::AsyncStopCursor{
    [cursor,terminal,owner=&owner]{
      event_loop::Deferred<query::AsyncStopStep> result(*owner);
      cursor.next().then([result,terminal](const std::optional<model::MessageRef>* value,
          const std::exception_ptr& error){
        if(error)result.reject(error);
        else if(value&&*value)result.resolve({false,**value,{}});
        else result.resolve({true,{},*terminal});
      });return result;
    },[cursor]{return cursor.close();}
  };
}
} // namespace batbox::application_engine
