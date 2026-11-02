#pragma once
#include "providers/aws/Credentials.hpp"
#include "providers/aws/EventStream.hpp"
#include "providers/cloud/HttpExchange.hpp"
#include "transport/WireClient.hpp"
#include <memory>
#include <mutex>
#include <string>

namespace batbox::providers::aws {
inline constexpr const char* bedrockAnthropicVersion = "bedrock-2023-05-31";
struct BedrockConfig {
  // Endpoint record region; when empty, AWS_REGION, AWS_DEFAULT_REGION, the profile region, then us-east-1.
  std::string region;
  // Optional bedrock-runtime override, for example a VPC interface endpoint.
  std::string baseUrl;
  // Bedrock API key from the endpoint record (auth_token); sent as a bearer token without signing.
  std::string bearerToken;
  Json headers = Json::object();
  // Timeouts, proxy and TLS settings.
  transport::ClientConfig network;
};
// The Anthropic body as Bedrock InvokeModel expects it: no model or stream, betas as anthropic_beta.
Json bedrockBody(const Json& anthropicBody);
std::string bedrockModelPath(const std::string& modelId, bool stream);
std::string bedrockRuntimeUrl(const BedrockConfig& config, const std::string& region);
std::string bedrockControlUrl(const std::string& region);
cloud::HttpRequest authorizeBedrockRequest(cloud::HttpRequest request, const BedrockAuth& auth, const std::string& region,
                                           std::chrono::system_clock::time_point now);
cloud::HttpRequest prepareBedrockRequest(const BedrockConfig& config, const BedrockAuth& auth, const std::string& region,
                                         const Json& anthropicBody, bool stream, std::chrono::system_clock::time_point now);
// Turns event-stream messages into Anthropic stream events; exceptions become Anthropic-shaped APIErrors.
class BedrockStreamTranslator {
  EventStreamDecoder decoder;
  Json headers;
public:
  explicit BedrockStreamTranslator(Json responseHeaders = Json::object()) : headers(std::move(responseHeaders)) {}
  void feed(std::string_view bytes, const transport::OnEvent& onEvent);
  void handle(const EventStreamMessage& message, const transport::OnEvent& onEvent);
  void finish() const;
};
[[noreturn]] void throwBedrockHttpError(long status, const std::string& body, Json headers);
class BedrockClient final : public transport::WireClient {
  BedrockConfig config;
  std::shared_ptr<CredentialChain> credentials;
  cloud::SystemClock now;
  cloud::HttpExchange http;
  std::mutex mutex;
  std::string region();
  cloud::HttpResult signedGet(const std::string& url, const BedrockAuth& auth, const std::string& signingRegion);
public:
  explicit BedrockClient(BedrockConfig config, CredentialOptions credentialOptions = {});
  // Clients sharing a chain share its stashed credentials.
  BedrockClient(BedrockConfig config, std::shared_ptr<CredentialChain> credentials);
  transport::Response request(const Json& anthropicBody, bool stream, const transport::OnEvent& onEvent = {},
                              std::stop_token stop = {}, const transport::OnResponse& onResponse = {},
                              transport::Timeout timeout = std::nullopt) override;
  Json listModels() override;
  std::string kind() override { return "bedrock"; }
};
} // namespace batbox::providers::aws
