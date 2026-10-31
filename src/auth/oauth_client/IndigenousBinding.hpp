#pragma once
#include "auth/oauth_client/AuthUrl.hpp"
#include "auth/oauth_client/TokenExchange.hpp"

namespace batbox::auth::oauth_client {
AuthUrlServices makeIndigenousAuthUrlServices(
  client_factory::oauth_config::Services config,
  std::vector<std::string> allScopes,std::string inferenceScope);
ExchangeServices makeIndigenousExchangeServices(event_loop::Loop& loop,
  client_factory::oauth_config::Services config);
}
