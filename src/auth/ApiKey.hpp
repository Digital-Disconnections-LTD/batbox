#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::auth {
struct ApiKeyResult {
  Json key=nullptr;
  std::string source="none";
};
struct ApiKeyKeys {
  std::string apiKey="BATBOX_API_KEY";
  std::string oauthToken="BATBOX_OAUTH_TOKEN";
  std::string oauthFd="BATBOX_OAUTH_TOKEN_FILE_DESCRIPTOR";
  std::string ci="CI";
  std::string nodeEnv="NODE_ENV";
};
struct ApiKeyServices {
  std::function<Json(const std::string&)> environment;
  std::function<bool()> bareMode,runningOnHomespace,
    preferThirdPartyAuthentication;
  std::function<Json()> flagSettings,mergedSettings,globalConfig;
  std::function<std::optional<std::string>()> apiKeyFromFd;
  std::function<Json()> apiKeyFromHelperStashed;
  std::function<std::optional<ApiKeyResult>()> loginManagedKey;
  ApiKeyKeys keys;
};
std::string normalizeApiKeyForConfig(const std::string& key);
Json getConfiguredApiKeyHelper(const ApiKeyServices& services);
ApiKeyResult getBatboxApiKeyWithSource(const ApiKeyServices& services,
  bool skipRetrievingKeyFromApiKeyHelper=false);
Json getBatboxApiKey(const ApiKeyServices& services);
bool hasBatboxApiKeyAuth(const ApiKeyServices& services);
}
