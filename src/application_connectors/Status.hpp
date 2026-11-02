#pragma once
#include "application/Config.hpp"
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace batbox::application_connectors {
// Where Batbox looks for a local SearXNG settings file: <config dir>/searxng/settings.yml.
std::filesystem::path searxngSettingsPath(const application::Config& config);
// The search endpoint a SearXNG settings file serves, read from its server
// block (port defaults to 8888, bind address to 127.0.0.1). Empty when the
// file does not exist.
std::string discoveredSearxngEndpoint(const std::filesystem::path& settings);
inline constexpr const char* defaultSearxngUrl="http://127.0.0.1:8888/search";

struct SearxngTarget {
  enum class Source {Configured,Discovered,Default};
  std::string url=defaultSearxngUrl;
  Source source=Source::Default;
  std::filesystem::path settings;
};
// searxng_url wins, then the settings file, then the default URL. Nothing is
// contacted; the default is only probed when a search runs.
SearxngTarget searxngTarget(const application::Config& config,
  std::filesystem::path settings={});

// What startup can know without starting a process or touching the network.
struct WebAvailability {
  std::string scraplingCommand;
  std::optional<std::filesystem::path> scrapling;
  SearxngTarget searxng;
  bool fetchReady()const{return scrapling.has_value();}
  bool searchConfigured()const{return searxng.source!=SearxngTarget::Source::Default;}
};
WebAvailability webAvailability(const application::Config& config);
// One line for the welcome card and /status; empty when both helpers look set up.
std::string webNotice(const WebAvailability& availability);
// One line for the model, added to the WebFetch and WebSearch descriptions.
std::string fetchUnavailableNote(const WebAvailability& availability);
std::string searchUnavailableNote(const WebAvailability& availability);

struct HelperCheck {
  std::string name,state,detail,fix;
  bool ok=false;
};
struct SearxngProbe {
  bool reachable=false,json=false;
  long status=0;
  std::size_t results=0;
  std::string error;
};
// One JSON search for "batbox" against the endpoint. Blocking.
SearxngProbe probeSearxng(const std::string& url,
  std::chrono::milliseconds timeout=std::chrono::seconds(5));
// Read from the installed package metadata beside the executable; no process runs.
std::optional<std::string> scraplingVersion(const std::filesystem::path& executable);
HelperCheck checkScrapling(const WebAvailability& availability);
HelperCheck checkSearxng(const WebAvailability& availability,const SearxngProbe& probe);
// Scrapling and SearXNG, probing SearXNG over the network. Blocking.
std::vector<HelperCheck> checkWebHelpers(const application::Config& config);

// MCP servers from the configuration (native-client.json or mcp.json).
Json configuredMcpServers(const application::Config& config);
// Checks each configured MCP server's command without starting it.
std::vector<HelperCheck> checkMcpConfiguration(const application::Config& config);
// A live server state from the MCP manager, with the fix for that state.
HelperCheck mcpServerCheck(const std::string& name,const std::string& type,
  const std::string& error);

std::string formatDoctor(const std::vector<HelperCheck>& web,
  const std::vector<HelperCheck>& mcp,const std::string& configurationError={},
  const std::string& title="Batbox doctor");
} // namespace batbox::application_connectors
