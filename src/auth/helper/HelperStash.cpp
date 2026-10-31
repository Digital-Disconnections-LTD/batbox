#include "auth/helper/Helper.hpp"

namespace batbox::auth::helper {
event_loop::Deferred<std::optional<std::string>> Stash::get(
    bool isNonInteractiveFlight){
  event_loop::Deferred<std::optional<std::string>> output(loop_);
  try{
  if(!value::truthy(services_.configuredHelper())){
    output.resolve(std::nullopt);return output;
  }
  const auto ttl=calculateTTL();
  if(stashed_){
    if(services_.nowMs()-stashed_->timestamp<ttl){
      output.resolve(stashed_->value);return output;
    }
    if(!inflight_)
      inflight_=Inflight{runAndStash(isNonInteractiveFlight,false,epoch_),
        std::nullopt};
    output.resolve(stashed_->value);return output;
  }
  if(inflight_)return event_loop::adoptAsyncReturn(inflight_->promise);
  auto promise=runAndStash(isNonInteractiveFlight,true,epoch_);
  inflight_=Inflight{promise,services_.nowMs()};
  return event_loop::adoptAsyncReturn(promise);
  }catch(...){output.reject(std::current_exception());return output;}
}
}
