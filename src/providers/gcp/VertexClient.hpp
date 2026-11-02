#pragma once
#include "providers/cloud/HttpExchange.hpp"
#include "transport/WireClient.hpp"
#include "providers/gcp/GoogleAuth.hpp"
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace batbox::providers::gcp {
inline constexpr const char* vertexAnthropicVersion = "vertex-2023-10-16";
struct VertexConfig {
  // GOOGLE_VERTEX_PROJECT, GOOGLE_CLOUD_PROJECT, GCP_PROJECT, GCLOUD_PROJECT, then the credentials file.
  std::string project;
  // GOOGLE_VERTEX_LOCATION, GOOGLE_CLOUD_LOCATION, VERTEX_LOCATION, then "global".
  std::string region;
  // Optional override of https://{region}-aiplatform.googleapis.com, for example a Private Service Connect endpoint.
  std::string baseUrl;
  // A ready access token (endpoint auth_token or api_key_helper output); skips ADC when set.
  std::string bearerToken;
  Json headers = Json::object();
  // Endpoint models[]; listModels falls back to the built-in Anthropic model list when empty.
  std::vector<std::string> models;
  transport::ClientConfig network;
};
const std::vector<std::string>& vertexAnthropicModels();
std::string vertexProject(const VertexConfig& config, const GoogleTokenSource* tokens = nullptr);
std::string vertexRegion(const VertexConfig& config);
std::string vertexHost(const std::string& region);
std::string vertexModelUrl(const VertexConfig& config, const std::string& project, const std::string& region,
                           const std::string& model, bool stream);
// The Anthropic body as rawPredict expects it: no model, anthropic_version added, stream set.
Json vertexBody(const Json& anthropicBody, bool stream);
cloud::HttpRequest prepareVertexRequest(const VertexConfig& config, const std::string& project, const std::string& region,
                                        const std::string& accessToken, const Json& anthropicBody, bool stream);
class VertexClient final : public transport::WireClient {
  VertexConfig config;
  std::shared_ptr<GoogleTokenSource> tokens;
  cloud::HttpExchange http;
  std::mutex mutex;
public:
  explicit VertexClient(VertexConfig config, GoogleAuthOptions authOptions = {});
  // Clients sharing a token source share its stashed access token.
  VertexClient(VertexConfig config, std::shared_ptr<GoogleTokenSource> tokens);
  transport::Response request(const Json& anthropicBody, bool stream, const transport::OnEvent& onEvent = {},
                              std::stop_token stop = {}, const transport::OnResponse& onResponse = {},
                              transport::Timeout timeout = std::nullopt) override;
  // Vertex has no inexpensive listing of Anthropic models, so this is the endpoint's models or the built-in list.
  Json listModels() override;
  std::string kind() override { return "vertex"; }
};
} // namespace batbox::providers::gcp
