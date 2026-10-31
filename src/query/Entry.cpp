#include "query/Entry.hpp"
#include <stdexcept>

namespace batbox::query {
Json query(const Json& params,const QueryLoop& queryLoop,
           const NotifyLifecycle& notifyCommandLifecycle,const Yield& yield) {
  if(!queryLoop)throw std::invalid_argument("Unbound queryLoop");
  if(!notifyCommandLifecycle)throw std::invalid_argument("Unbound notifyCommandLifecycle");
  if(!yield)throw std::invalid_argument("Unbound yield");
  std::vector<std::string> consumedCommandUuids;
  const Json terminal=queryLoop(params,consumedCommandUuids,yield);
  for(const auto& uuid:consumedCommandUuids)
    notifyCommandLifecycle(uuid,"completed");
  return terminal;
}
} // namespace batbox::query
