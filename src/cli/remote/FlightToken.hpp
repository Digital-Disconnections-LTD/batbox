#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>

namespace batbox::cli::remote {
Json decodeFlightJwtPayload(const std::string& token,
  const std::string& configuredPrefix);
std::optional<double> decodeFlightJwtExpiry(const std::string& token,
  const std::string& configuredPrefix);
}
