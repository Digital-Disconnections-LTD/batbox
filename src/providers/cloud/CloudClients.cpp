#include "providers/cloud/CloudClients.hpp"
#include "providers/aws/BedrockClient.hpp"
#include "providers/cloud/Crypto.hpp"
#include "providers/gcp/VertexClient.hpp"
#include <map>
#include <mutex>
#include <stdexcept>

namespace batbox::providers::cloud {
namespace {
std::mutex registryMutex;
template <class T, class Make> std::shared_ptr<T> shared(std::map<std::string,std::shared_ptr<T>>& registry, const std::string& key, Make make) {
  std::lock_guard lock(registryMutex);
  auto& slot = registry[key];
  if (!slot) slot = make();
  return slot;
}
} // namespace
std::shared_ptr<transport::WireClient> makeCloudClient(const CloudEndpoint& endpoint, transport::ClientConfig network) {
  const auto baseUrl = network.baseUrl;
  auto headers = network.headers.is_object() ? network.headers : Json::object();
  for (const auto* name : {"authorization","x-api-key","anthropic-version"}) headers.erase(name);
  network.authToken.clear();
  const auto key = endpoint.kind+"\n"+sha256Hex(endpoint.token);
  if (endpoint.kind == transport::kProviderBedrock) {
    static std::map<std::string,std::shared_ptr<aws::CredentialChain>> chains;
    aws::BedrockConfig config;
    config.region = endpoint.region;
    config.baseUrl = baseUrl;
    config.bearerToken = endpoint.token;
    config.headers = std::move(headers);
    config.network = network;
    auto chain = shared(chains,key,[&] {
      aws::CredentialOptions options;
      options.bearerToken = endpoint.token;
      options.network = network;
      return std::make_shared<aws::CredentialChain>(std::move(options));
    });
    return std::make_shared<aws::BedrockClient>(std::move(config),std::move(chain));
  }
  if (endpoint.kind == transport::kProviderVertex) {
    static std::map<std::string,std::shared_ptr<gcp::GoogleTokenSource>> sources;
    gcp::VertexConfig config;
    config.project = endpoint.project;
    config.region = endpoint.region;
    config.baseUrl = baseUrl;
    config.bearerToken = endpoint.token;
    config.headers = std::move(headers);
    config.models = endpoint.models;
    config.network = network;
    auto source = shared(sources,key,[&] {
      gcp::GoogleAuthOptions options;
      options.network = network;
      return std::make_shared<gcp::GoogleTokenSource>(std::move(options));
    });
    return std::make_shared<gcp::VertexClient>(std::move(config),std::move(source));
  }
  throw std::invalid_argument("Not a cloud provider kind: "+endpoint.kind);
}
} // namespace batbox::providers::cloud
