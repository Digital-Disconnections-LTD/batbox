#include "application_hooks/Provider.hpp"

namespace batbox::application_hooks {
event_loop::Deferred<std::vector<Json>> Provider::run(const std::string& event,
    Json detail,std::string matchQuery,std::stop_token signal,
    std::optional<execution::Context> queryContext,
    std::optional<Json> hookMessages){
  auto result=runSource(event,std::move(detail),std::move(matchQuery),signal,
    std::move(queryContext),std::move(hookMessages));
  if(event!="SessionStart"&&event!="Setup")return result;
  // These hooks may have rewritten their environment files.
  event_loop::Deferred<std::vector<Json>> output(owner_);
  result.then([output,environment=flightEnvironment_](const std::vector<Json>* events,
      const std::exception_ptr& error){
    try{environment->invalidateFlightEnvStash();}
    catch(...){output.reject(std::current_exception());return;}
    if(error)output.reject(error);else output.resolve(*events);
  });
  return output;
}
} // namespace batbox::application_hooks
