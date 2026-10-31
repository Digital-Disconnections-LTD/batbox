#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>
#include <string>

namespace batbox::settings {
struct ValidationTip {
  std::optional<std::string> suggestion,docLink;
};
ValidationTip getValidationTip(const Json& context,
  const std::string& docsBase);
Json formatZodError(const Json& issues,const std::string& filePath,
  const std::string& docsBase);
}
