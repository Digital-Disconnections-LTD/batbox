#pragma once
#include "settings/remote/FetchTypes.hpp"

namespace batbox::settings::remote {
struct AuthServices {
  std::function<std::optional<std::string>()> apiKeyWithoutHelper;
  std::function<Json()> oauthTokens;
  std::string apiKeyHeader="x-api-key";
  std::string oauthBetaHeader="batbox-beta";
  std::string oauthBetaValue;
};
std::string getRemoteManagedSettingsEndpoint(const std::string& baseApiUrl,
  const std::string& settingsPath="/api/batbox/settings");
AuthHeaders getRemoteSettingsAuthHeaders(const AuthServices& services);
}
