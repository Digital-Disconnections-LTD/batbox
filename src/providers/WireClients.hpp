#pragma once
#include "application/Config.hpp"
#include "transport/WireClient.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace batbox::providers {
// What a wire client needs besides its network settings.
struct WireTarget {
  std::string kind=std::string(transport::kProviderAnthropic);
  Json quirks=Json::object();
  std::string region,project;
  std::vector<std::string> models;
};
// The one place a provider kind becomes a client, for requests and model
// listings alike. config already carries the auth a request sends (applyAuth).
std::shared_ptr<transport::WireClient> makeWireClient(const WireTarget& target,
  transport::ClientConfig config);
// Puts the key where auth_header says: a bearer token, an x-api-key header
// (plus anthropic-version for Anthropic servers) or nothing.
void applyAuth(transport::ClientConfig& config,const std::string& kind,
  const std::string& authHeader,const std::string& token);
// auth_token, else the api_key_env variable; empty for auth_header none.
std::string endpointKey(const application::EndpointRecord& endpoint);
// Network settings for one endpoint record, bounded for a model listing.
transport::ClientConfig endpointConfig(const application::EndpointRecord& endpoint,
  std::chrono::milliseconds timeout=std::chrono::seconds(8));
// The model list the endpoint's wire client reports, {"data":[{"id":...}]}.
// Throws transport::APIConnectionError when the server cannot be reached and
// transport::APIError with the HTTP status when it refuses.
Json listEndpointModels(const application::EndpointRecord& endpoint,
  std::chrono::milliseconds timeout=std::chrono::seconds(8));
using ListModels=std::function<Json(const application::EndpointRecord&)>;
// What a listing reaches, for progress text: "GET http://127.0.0.1:11434/v1/models",
// "Amazon Bedrock in us-west-2 (signed with your AWS credentials)".
std::string listingTarget(const application::EndpointRecord& endpoint);
} // namespace batbox::providers
