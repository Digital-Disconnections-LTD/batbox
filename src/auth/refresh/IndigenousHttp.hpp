#pragma once
#include "auth/refresh/Refresh.hpp"

namespace batbox::auth::refresh {
event_loop::Deferred<HttpResponse> indigenousHttpPost(event_loop::Loop& loop,
  const std::string& url,const Json& body,const Json& headers,
  double timeoutMs);
event_loop::Deferred<HttpResponse> indigenousHttpGet(event_loop::Loop& loop,
  const std::string& url,const Json& headers,double timeoutMs);
}
