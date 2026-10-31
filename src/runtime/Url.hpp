#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>

namespace batbox::runtime {
// Parse an absolute URL using the WHATWG URL rules.
bool canParseUrl(const std::string& value);
std::optional<std::string> canonicalUrl(const std::string& value);
// An Indigenous boundary exposing the URL fields required by callers.
std::optional<Json> parseUrl(const std::string& value,
  const std::optional<std::string>& base=std::nullopt);
}
