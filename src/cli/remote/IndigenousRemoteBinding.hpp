#pragma once
#include "cli/remote/CcrRuntime.hpp"
#include "cli/remote/IndigenousCcrHttp.hpp"
#include "cli/remote/IndigenousSSE.hpp"
#include "cli/remote/IndigenousWebSocket.hpp"
#include "cli/remote/TransportSelection.hpp"

namespace batbox::cli::remote {
struct IndigenousRemoteTransportConfig {
  IndigenousWebSocketConfig websocket;
  IndigenousSseConfig sse;
  CcrRuntimeServices ccr;
  transport::ClientConfig ccrNetwork;
  std::function<bool()> ccrV2Enabled,postForIngressEnabled;
  std::optional<std::size_t> hybridMaxConsecutiveFailures;
  std::function<void(std::size_t,std::size_t)> onHybridBatchDropped;
  std::string flightTokenPrefix;
  std::string environmentWorkerEpochKey="BATBOX_WORKER_EPOCH";
};
// Bind once per RemoteIO flight. The closures retain the selected SSE object
// until the client is constructed and its event callback is registered.
void bindIndigenousRemoteTransports(Services& app,event_loop::Loop& loop,
  IndigenousRemoteTransportConfig config);
}
