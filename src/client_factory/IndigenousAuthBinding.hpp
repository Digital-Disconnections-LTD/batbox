#pragma once
#include "auth/ApiKey.hpp"
#include "auth/OAuthTokens.hpp"
#include "auth/helper/Helper.hpp"
#include "auth/refresh/Check.hpp"
#include "client_factory/ClientFactory.hpp"
#include <memory>

namespace batbox::client_factory {
struct IndigenousAuthSources {
  event_loop::Loop* loop=nullptr;
  std::shared_ptr<auth::refresh::CheckController> refresh;
  std::shared_ptr<auth::helper::Stash> apiKeyHelper;
  std::shared_ptr<auth::OAuthTokens> oauthTokens;
  auth::ApiKeyServices apiKey;
  std::function<bool()> authEnabled;
  std::string inferenceScope="user:inference";
};
bool isBatboxSubscriber(const std::function<bool()>& authEnabled,
  const std::function<std::optional<Json>()>& oauthTokens,
  const std::string& inferenceScope="user:inference");
// Binds authentication modules to the client factory without global
// singletons or dummy credentials.
void bindIndigenousAuth(Services& client,IndigenousAuthSources sources);
}
