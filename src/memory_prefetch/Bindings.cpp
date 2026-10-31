#include "memory_prefetch/Bindings.hpp"
#include <stdexcept>

namespace batbox::memory_prefetch {
void bindSideQuery(Services& memory,side_query::Services side){
  auto& loop=memory.loop;
  memory.sideQuery=[&loop,side=std::move(side)](const Json& request,std::stop_token signal){
    event_loop::Deferred<Json> output(loop);
    auto pending=side_query::sideQuery(loop,request,signal,side);
    pending.then([output](const side_query::ResponseRef* value,
        const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      output.resolve(**value);
    });
    return output;
  };
}
void bindQueryPrefetch(query::AsyncEngineServices& engine,
    query::AsyncGadgetTurnServices& gadgetTurn,Services memory){
  engine.startRelevantMemoryPrefetch=[memory](const Json& messages,
      const execution::Context& context){
    return startRelevantMemoryPrefetch(messages,context,memory);
  };
  gadgetTurn.readMemoryPrefetch=[&loop=memory.loop](query::MemoryPrefetch& prefetch){
    event_loop::Deferred<Json> output(loop);
    try{
      if(!prefetch.read)throw std::invalid_argument("Memory prefetch read required");
      output.resolve(prefetch.read());
    }catch(...){output.reject(std::current_exception());}
    return output;
  };
}
} // namespace batbox::memory_prefetch
