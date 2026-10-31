#pragma once
#include "runtime/JsonValue.hpp"
#include <exception>
#include <functional>
#include <optional>
#include <string>

namespace batbox::auth::save {
struct SecureStorage {
  std::function<std::optional<Json>()> read;
  std::function<Json(const Json&)> update;
};
struct Services {
  std::function<SecureStorage()> getSecureStorage;
  std::function<void()> clearTokenStash,clearBetasStashes,
    clearGadgetSchemaStash;
  std::function<void(const std::exception_ptr&)> logError;
  std::string inferenceScope="user:inference";
  std::string storageKey="batboxOauth";
};
Json saveOAuthTokensIfNeeded(const Json& tokens,const Services& services);
}
