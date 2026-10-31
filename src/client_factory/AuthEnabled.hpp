#pragma once
#include "auth/ApiKey.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <string>

namespace batbox::client_factory {
struct AuthEnabledKeys {
  std::string remote="BATBOX_REMOTE";
  std::string entrypoint="BATBOX_ENTRYPOINT";
  std::string desktopEntrypoint="batbox-desktop";
  std::string unixSocket="BATBOX_UNIX_SOCKET";
  std::string oauthToken="BATBOX_OAUTH_TOKEN";
  std::string useBedrock="BATBOX_USE_BEDROCK";
  std::string useVertex="BATBOX_USE_VERTEX";
  std::string useFoundry="BATBOX_USE_FOUNDRY";
  std::string authToken="BATBOX_AUTH_TOKEN";
  std::string apiKeyFd="BATBOX_API_KEY_FILE_DESCRIPTOR";
  std::string externalApiKeySource="BATBOX_API_KEY";
};
struct AuthEnabledServices {
  std::function<Json(const std::string&)> environment;
  std::function<bool()> bareMode;
  std::function<Json()> mergedSettings;
  std::function<auth::ApiKeyResult(bool)> apiKeyWithSource;
  AuthEnabledKeys keys;
};
bool isManagedAuthContext(const AuthEnabledServices& services);
bool isBatboxAuthEnabled(const AuthEnabledServices& services);
}
