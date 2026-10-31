#pragma once
#include "auth/refresh/Refresh.hpp"

namespace batbox::auth::oauth_client {
event_loop::Deferred<auth::refresh::HttpResponse> indigenousEmptyPost(
  event_loop::Loop& loop,const std::string& url,const Json& headers);
}
