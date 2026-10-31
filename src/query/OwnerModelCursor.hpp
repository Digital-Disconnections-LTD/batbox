#pragma once
#include "query/IndigenousModelCursor.hpp"
#include "query/OwnerPreflight.hpp"
#include "query/OwnerRetry.hpp"

namespace batbox::query {
struct OwnerRequestAwaits {
  // Retry awaits client creation. Creation starts on the owner and
  // may settle later; the request worker waits without pumping a nested loop.
  std::function<event_loop::Deferred<std::shared_ptr<transport::Client>>()>
    createClient;
  OwnerRetryAwaits retry;
};

// Binds await stages and complete synchronous segments to the owner.
// Blocking HTTP/SSE work remains on an Indigenous worker.
AsyncModelCursor makeOwnerSegmentQueryModelCursor(event_loop::Loop& loop,
  model::PreflightInputs input,model::QueryServices services,
  OwnerPreflightAwaits awaitedImports={},
  const std::function<void()>& onStreamingFallback={},
  OwnerRequestAwaits requestAwaits={});
} // namespace batbox::query
