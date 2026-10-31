#pragma once
#include "cli/remote/WebSocketTransport.hpp"
#include <optional>

namespace batbox::cli::remote {
struct IndigenousWebSocketConfig {
  std::function<std::optional<std::string>(const std::string&)> getProxyUrl;
  std::function<std::optional<Json>()> getTlsOptions;
  std::function<double()> nowMs,randomUnit;
  std::function<void(const std::string&,const Json&)> logDebug;
  std::function<void(std::function<void()>)> registerFlightActivity;
  std::function<void()> unregisterFlightActivity;
  bool remoteMode=false;
};
WebSocketServices makeIndigenousWebSocketServices(event_loop::Loop& loop,
  IndigenousWebSocketConfig config);
}
