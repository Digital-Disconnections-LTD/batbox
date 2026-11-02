#pragma once
#include "providers/cloud/CloudKinds.hpp"
#include "transport/WireClient.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace batbox::providers::cloud {
struct CloudEndpoint {
  std::string kind;   // transport::kProviderBedrock or transport::kProviderVertex
  std::string region;
  std::string project;
  std::vector<std::string> models;
  // The endpoint's resolved key (auth_token, api_key_env or api_key_helper); may be empty.
  std::string token;
};
// network supplies base_url, headers, timeouts, proxy and TLS. Clients built for the
// same kind and key share one credential stash, so credential_process, STS and token
// exchanges run once per expiry instead of once per request.
std::shared_ptr<transport::WireClient> makeCloudClient(const CloudEndpoint& endpoint, transport::ClientConfig network);
} // namespace batbox::providers::cloud
