#pragma once
#include "auth/refresh/Refresh.hpp"
#include "client_factory/oauth_config/OAuthConfig.hpp"
#include "event_loop/Deferred.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::auth::oauth_client {
struct AccountServices {
  std::function<client_factory::oauth_config::Config()> getConfig;
  std::function<event_loop::Deferred<auth::refresh::HttpResponse>(
    const std::string&,const Json&)> httpGet;
  std::function<event_loop::Deferred<auth::refresh::HttpResponse>(
    const std::string&,const Json&,const Json&)> httpPost;
  std::function<Json()> getGlobalConfig;
  std::function<void(const std::function<Json(const Json&)>&)>
    saveGlobalConfig;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&)>
    saveApiKey;
};
event_loop::Deferred<event_loop::Unit> fetchAndStoreUserRoles(
  event_loop::Loop& loop,const std::string& accessToken,
  const AccountServices& services);
event_loop::Deferred<std::optional<std::string>> createAndStoreApiKey(
  event_loop::Loop& loop,const std::string& accessToken,
  const AccountServices& services);
}
