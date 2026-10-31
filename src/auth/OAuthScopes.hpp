#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::auth {
bool shouldUseBatboxAuth(const std::optional<std::vector<std::string>>& scopes,
  const std::string& inferenceScope);
std::vector<std::string> parseScopes(
  const std::optional<std::string>& scopeString);
bool isOAuthTokenExpired(const std::optional<double>& expiresAt,
  const std::function<double()>& nowMs);
}
