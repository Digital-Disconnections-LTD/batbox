#include "settings/remote/Retry.hpp"
#include <memory>
#include <stdexcept>

namespace batbox::settings::remote {
event_loop::Deferred<FetchResult> fetchWithRetry(event_loop::Loop& loop,
    const std::optional<std::string>& stashedChecksum,
    const RetryServices& services){
  if(!services.fetchOne||!services.getRetryDelay||!services.sleep||
      !services.debug)
    throw std::invalid_argument("Missing remote retry binding");
  event_loop::Deferred<FetchResult> output(loop);
  struct State {
    event_loop::Deferred<FetchResult> output;
    std::optional<std::string> checksum;
    RetryServices services;
    int attempt=1;
    std::function<void()> run;
  };
  auto state=std::make_shared<State>(State{output,stashedChecksum,services,1,{}});
  std::weak_ptr<State> weak=state;
  state->run=[weak](){
    const auto state=weak.lock();
    if(!state)return;
    try{
      state->services.fetchOne(state->checksum).then([state](
          const FetchResult* value,const std::exception_ptr& error){
        if(error){state->output.reject(error);return;}
        if(value->success||value->skipRetry||
            state->attempt>state->services.maxRetries){
          state->output.resolve(*value);return;
        }
        try{
          const auto delay=state->services.getRetryDelay(state->attempt);
          state->services.debug("Remote settings: Retry "+
            std::to_string(state->attempt)+"/"+
            std::to_string(state->services.maxRetries)+" after "+
            value::numberToString(delay)+"ms");
          ++state->attempt;
          state->services.sleep(delay).then([state](
              const event_loop::Unit*,const std::exception_ptr& sleepError){
            if(sleepError)state->output.reject(sleepError);
            else state->run();
          });
        }catch(...){state->output.reject(std::current_exception());}
      });
    }catch(...){state->output.reject(std::current_exception());}
  };
  state->run();
  return output;
}
}
