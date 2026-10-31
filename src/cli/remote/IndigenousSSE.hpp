#pragma once
#include "cli/remote/SSETransport.hpp"
#include "transport/Client.hpp"

namespace batbox::cli::remote {
struct IndigenousSseConfig {
  transport::ClientConfig network;
  std::function<Json()> getAuthHeaders;
  std::function<std::string()> userBat;
  std::function<void(const std::string&,const Json&)> logDebug;
  std::string protocolVersionHeaderName,protocolVersionHeaderValue;
};
SseServices makeIndigenousSseServices(event_loop::Loop& loop,
  IndigenousSseConfig config);
}
