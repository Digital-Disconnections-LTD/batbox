#pragma once
#include "mcp/auth/Provider.hpp"
#include "auth/oauth_listener/AuthCodeListener.hpp"
#include <chrono>

namespace batbox::mcp::auth {
struct PortServices {
  std::function<bool(int)> available;
  std::function<double()> random;
  std::optional<std::string> configured;
  bool windows=false;
};
int findAvailablePort(const PortServices& services);
struct FlowInputs {
  event_loop::Loop& owner;
  std::string name;
  Json config;
  Services services;
  std::stop_token signal;
  bool skipBrowserOpen=false;
  std::function<void(std::function<void(const std::string&)>)> onWaitingForCallback;
  std::function<std::shared_ptr<::batbox::auth::oauth_listener::Server>()> createServer;
  std::function<int()> findPort;
  explicit FlowInputs(event_loop::Loop& loop):owner(loop){}
};
event_loop::Deferred<event_loop::Unit> performMCPOAuthFlow(FlowInputs inputs);
} // namespace batbox::mcp::auth
