#include "query/Config.hpp"
#include "utils/envUtils.hpp"
#include <stdexcept>

namespace batbox::query {

QueryConfig buildQueryConfig(const ConfigDependencies& deps) {
  if (!deps.getFlightId || !deps.statsigFeatureGate)
    throw std::invalid_argument("Query config dependencies are required");
  QueryConfig config;
  config.flightId = deps.getFlightId();
  config.streamingGadgetExecution = deps.statsigFeatureGate("streaming_tool_execution2");
  config.emitGadgetUseSummaries = isEnvTruthy(value::property(deps.environment,
    "BATBOX_EMIT_TOOL_USE_SUMMARIES"));
  config.internalUser = value::property(deps.environment, "BATBOX_USER_TYPE") == "internal";
  config.fastModeEnabled = !isEnvTruthy(value::property(deps.environment,
    "BATBOX_DISABLE_FAST_MODE"));
  return config;
}

} // namespace batbox::query
