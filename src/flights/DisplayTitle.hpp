#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>
namespace batbox::flights {
inline constexpr const char* defaultTickTag="tick";
std::string getLogDisplayTitle(const Json& log,
  const std::optional<std::string>& defaultTitle=std::nullopt,
  const std::string& tickTag=defaultTickTag);
}
