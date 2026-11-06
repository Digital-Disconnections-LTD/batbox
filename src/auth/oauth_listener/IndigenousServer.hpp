#pragma once
#include "auth/oauth_listener/CallbackServer.hpp"

namespace batbox::auth::oauth_listener {
std::shared_ptr<Server> makeIndigenousCallbackServer(event_loop::Loop& loop);
}
