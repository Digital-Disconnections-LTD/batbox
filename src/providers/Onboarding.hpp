#pragma once
#include "application/Config.hpp"
#include "mcp/http/Http.hpp"
#include "providers/Catalog.hpp"
#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace batbox::providers {
using Environment=std::function<std::optional<std::string>(const std::string&)>;
Environment processEnvironment();

// What a person chose while adding a provider.
struct Setup {
  std::string baseUrl;
  // At most one of these: an environment variable name, or the key itself.
  std::string keyEnv,key;
  std::string model;
  // Live model ids from the server, and context lengths it reported.
  std::vector<std::string> liveModels;
  std::map<std::string,std::size_t> liveContexts;
};

// The catalog base URL with ${VAR} placeholders filled from the environment.
std::string resolvedBaseUrl(const CatalogProvider& provider,const Environment& environment);
// Placeholders still present ("{resource}", "${VAR}") that the person must edit.
bool needsBaseUrlEdit(const std::string& baseUrl);
// Key variables from the catalog that are set and nonempty.
std::vector<std::string> presentKeyEnv(const CatalogProvider& provider,
  const Environment& environment);
// Whether the provider takes an API key at all.
bool takesKey(const CatalogProvider& provider);

application::EndpointRecord buildEndpoint(const CatalogProvider& provider,const Setup& setup,
  std::size_t defaultContext=131072,std::size_t defaultOutput=16384);
// "openrouter", or "openrouter-2" when taken.
std::string uniqueEndpointId(const std::map<std::string,application::EndpointRecord>& endpoints,
  const std::string& base);

struct ConnectionResult {
  bool reached=false,ok=false;
  int status=0;
  std::vector<std::string> models;
  std::string message;
};
// Lists models; never sends a chat request. Bedrock and Vertex are signed per
// request and are not tested here.
ConnectionResult testConnection(const application::EndpointRecord& endpoint,
  mcp::http::Fetch fetch=mcp::http::fetch,
  std::chrono::milliseconds timeout=std::chrono::seconds(8));

struct SaveResult {
  std::string endpointId,profile;
  application::EndpointRecord endpoint;
  std::map<std::string,application::EndpointRecord> endpoints;
};
// Adds or replaces one endpoint, selects endpointId::model as the profile and
// writes native-client.json atomically with mode 0600.
SaveResult saveEndpoint(const std::filesystem::path& path,
  const std::filesystem::path& legacyPath,
  std::map<std::string,application::EndpointRecord> endpoints,
  const std::string& endpointId,application::EndpointRecord endpoint,const std::string& model);
// "Added openrouter · qwen/qwen3-coder · /model to switch"
std::string addedNotice(const std::string& endpointId,const std::string& model);
} // namespace batbox::providers
