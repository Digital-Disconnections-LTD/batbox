#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::auth::oauth_client {
bool hasProfileScope(const std::function<std::optional<Json>()>& getTokens,
  const std::string& profileScope);
}
