#pragma once
#include "client_factory/oauth_config/OAuthConfig.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::auth::oauth_client {
struct AuthUrlArgs {
  std::string codeChallenge,state;
  int port=0;
  bool isManual=false,loginWithBatbox=false,inferenceOnly=false;
  std::optional<std::string> orgUUID,loginHint,loginMethod;
};
struct AuthUrlServices {
  std::function<client_factory::oauth_config::Config()> getConfig;
  std::vector<std::string> allScopes;
  std::string inferenceScope;
};
std::string buildAuthUrl(const AuthUrlArgs& args,
  const AuthUrlServices& services);
}
