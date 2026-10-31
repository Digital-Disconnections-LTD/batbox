#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::auth {
struct OAuthTokenServices {
  std::function<bool()> isBareMode;
  std::function<std::optional<std::string>(const std::string&)> environment;
  std::function<std::optional<std::string>()> oauthTokenFromFd;
  std::function<std::optional<Json>()> secureStorageRead;
  std::function<event_loop::Deferred<std::optional<Json>>()> secureStorageReadAsync;
  std::function<void(const std::string&)> logError;
  std::function<void()> clearKeychainStash;
  std::string oauthEnv="BATBOX_OAUTH_TOKEN";
  std::string storageKey="batboxOauth";
  std::string inferenceScope="user:inference";
};
class OAuthTokens {
public:
  OAuthTokens(event_loop::Loop& loop,OAuthTokenServices services);
  std::optional<Json> getBatboxOAuthTokens();
  event_loop::Deferred<std::optional<Json>> getBatboxOAuthTokensAsync();
  void clearOAuthTokenStash();
private:
  event_loop::Loop& loop_;
  OAuthTokenServices services_;
  std::optional<std::optional<Json>> memoized_;
  std::optional<Json> readStored(const std::optional<Json>& storage)const;
};
}
