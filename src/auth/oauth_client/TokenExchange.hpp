#pragma once
#include "auth/refresh/Refresh.hpp"
#include "client_factory/oauth_config/OAuthConfig.hpp"
#include "event_loop/Deferred.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::auth::oauth_client {
struct ExchangeArgs {
  std::string authorizationCode,state,codeVerifier;
  int port=0;
  bool useManualRedirect=false;
  std::optional<double> expiresIn;
};
struct ExchangeServices {
  std::function<client_factory::oauth_config::Config()> getConfig;
  std::function<event_loop::Deferred<auth::refresh::HttpResponse>(
    const std::string&,const Json&,const Json&,double)> httpPost;
};
event_loop::Deferred<Json> exchangeCodeForTokens(event_loop::Loop& loop,
  const ExchangeArgs& args,const ExchangeServices& services);
}
