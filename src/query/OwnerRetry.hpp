#pragma once

#include "event_loop/Deferred.hpp"
#include "retry/Retry.hpp"

namespace batbox::query {

struct OwnerRetryAwaits {
  // Complete OAuth 401 recovery before requesting a client for an auth retry.
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&)>
    refreshOAuth;
  std::function<event_loop::Deferred<event_loop::Unit>(double,std::stop_token)>
    sleep;
};

// Drives the shared retry transition machine on the query owner. Only actual
// awaited client/operation/sleep work and yielded values run on its worker.
Json withRetryOwnerSegments(event_loop::Loop& loop,
  const retry::RetryOptions& options,retry::Dependencies& deps,
  const OwnerRetryAwaits& awaits={},std::stop_token cursorClose={});

} // namespace batbox::query
