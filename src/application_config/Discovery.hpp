#pragma once
#include "application/Config.hpp"
#include "mcp/http/Http.hpp"
#include <functional>
#include <string>
#include <vector>

namespace batbox::application_config {
struct DiscoveryResult {
  std::vector<std::string> models;
  std::string error;
  bool success=false;
};
// Blocking HTTP; call from a worker. The injectable fetch makes offline and
// unusual compatible-server responses testable without a live inference server.
DiscoveryResult discoverModels(const application::EndpointRecord& endpoint,
  mcp::http::Fetch fetch=mcp::http::fetch);
}
