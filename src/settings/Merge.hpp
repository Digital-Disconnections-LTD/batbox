#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>

namespace batbox::settings {
std::optional<Json> settingsMergeCustomizer(const Json& target,
  const Json& source);
Json mergeSettings(const Json& target,const Json& source);
}
