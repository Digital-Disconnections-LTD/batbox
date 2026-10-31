#pragma once
#include "auth/oauth_listener/AuthCodeListener.hpp"

namespace batbox::auth::oauth_listener {
std::shared_ptr<Server> makeIndigenousCallbackServer(event_loop::Loop& loop);
Services makeIndigenousListenerServices(event_loop::Loop& loop,
  client_factory::oauth_config::Services config,
  std::function<void(const std::string&)> logError,
  std::string inferenceScope);
}
