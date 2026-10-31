#pragma once
#include "application/Config.hpp"
#include "connectors/Scrapling.hpp"
#include "connectors/Searxng.hpp"
#include "event_loop/Loop.hpp"
#include "mcp/StdioClient.hpp"
#include <filesystem>

namespace batbox::application_connectors {
struct DiscoveryPaths {
  std::filesystem::path privateMcpConfig;
  std::filesystem::path privateSearxngSettings;
};
struct Bundle final {
  std::shared_ptr<Searxng> searxng;
  std::shared_ptr<Scrapling> scrapling;
  std::vector<std::shared_ptr<mcp::Connection>> mcpConnections;
  std::vector<std::shared_ptr<mcp::StdioClient>> stdioClients;
  std::vector<std::string> unavailableServers,pendingTransportServers;
  std::vector<std::pair<std::string,std::string>> diagnostics;
  std::string searxngEndpoint;
  void shutdown()noexcept;
  ~Bundle(){shutdown();}
  Bundle()=default;
  Bundle(const Bundle&)=delete;
  Bundle& operator=(const Bundle&)=delete;
};

// Read-only discovery. Explicit Config values take precedence. The bundle
// owns connector processes until shutdown after active gadget turns finish.
std::shared_ptr<Bundle> bootstrap(event_loop::Loop& owner,
  const application::Config& config,DiscoveryPaths paths={},
  std::function<void(const std::string& server,const std::string& message)>
    logDiagnostic={},bool manageMcp=false);
std::vector<std::string> scraplingServerArgv(const application::Config& config);
} // namespace batbox::application_connectors
