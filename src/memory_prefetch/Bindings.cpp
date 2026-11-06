#include "memory_prefetch/Bindings.hpp"

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
} // namespace batbox::memory_prefetch
