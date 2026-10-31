#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <string>

namespace batbox::query {

struct QueryConfig {
  std::string flightId;
  bool streamingGadgetExecution = false;
  bool emitGadgetUseSummaries = false;
  bool internalUser = false;
  bool fastModeEnabled = true;
};

struct ConfigDependencies {
  std::function<std::string()> getFlightId;
  std::function<bool(const std::string&)> statsigFeatureGate;
  Json environment = Json::object();
};

QueryConfig buildQueryConfig(const ConfigDependencies& deps);

} // namespace batbox::query
