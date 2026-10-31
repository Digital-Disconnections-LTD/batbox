#pragma once
#include "auth/oauth_client/ProfileAccount.hpp"
#include "auth/refresh/Check.hpp"
#include "client_factory/oauth_config/OAuthConfig.hpp"
#include <memory>

namespace batbox::auth::oauth_client {
ProfileAccountServices makeIndigenousProfileAccountServices(
  event_loop::Loop& loop,
  client_factory::oauth_config::Services config,
  std::shared_ptr<auth::refresh::CheckController> checker,
  std::function<std::optional<Json>()> getOAuthTokens,
  std::function<Json()> getGlobalConfig,
  std::function<void(const std::function<Json(const Json&)>&)>
    saveGlobalConfig,
  std::function<Json(const std::string&)> environment,
  std::function<bool()> isSubscriber,
  std::function<void(const std::string&)> logForDebugging,
  std::function<void(const std::string&)> logProfileError,
  std::string profileScope);
}
