#pragma once
#include "application/Config.hpp"
#include "providers/WireClients.hpp"
#include <map>
#include <string>
#include <vector>

namespace batbox::application_config {
struct DiscoveryResult {
  std::vector<std::string> models;
  std::string error;
  bool success=false;
  // Context length per model when the model list reports one.
  std::map<std::string,std::size_t> contexts;
};
// Blocking; call from a worker. The endpoint's own wire client lists the
// models, so every provider kind answers in its own way. The injectable list
// makes offline and unusual replies testable without a live server.
DiscoveryResult discoverModels(const application::EndpointRecord& endpoint,
  providers::ListModels list={});
}
