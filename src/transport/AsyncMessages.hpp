#pragma once
#include "event_loop/Deferred.hpp"
#include "transport/Client.hpp"
#include "transport/SdkRetry.hpp"

namespace batbox::transport {
// The indigenous non-streaming equivalent of the SDK request/response Promise.
// Blocking network I/O runs outside the owner loop; retry timers stay on it.
event_loop::Deferred<Response> createMessage(event_loop::Loop& loop,
  std::shared_ptr<Client> client,Json body,int maxRetries,std::stop_token signal,
  SdkRetryClock clock=indigenousSdkRetryClock(),SdkRetryAuth auth={});
} // namespace batbox::transport
