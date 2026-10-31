#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::commands_indigenous {
std::string compactDisplayText(bool verbose,const std::string& upgradeTip,
  const std::string& expandShortcut,
  const std::optional<std::string>& userDisplayMessage,
  const std::function<std::string(const std::string&)>& dim);
Json filterResumableFlights(const Json& logs,
  const std::string& currentFlightId,
  const std::function<std::string(const Json&)>& flightIdFromLog);
} // namespace batbox::commands_indigenous
