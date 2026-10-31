#include "application_hooks/Provider.hpp"

namespace batbox::application_hooks {
event_loop::Deferred<std::vector<Json>> Provider::run(const std::string& event,
    Json detail,std::string matchQuery,std::stop_token signal,
    std::optional<execution::Context> queryContext,
    std::optional<Json> hookMessages){
  return runSource(event,std::move(detail),std::move(matchQuery),signal,
    std::move(queryContext),std::move(hookMessages));
}
} // namespace batbox::application_hooks
