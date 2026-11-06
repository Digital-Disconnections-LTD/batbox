#pragma once
#include "runtime/JsonValue.hpp"
#include "transport/Client.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace batbox::transport {

// Wire kinds a profile's "provider" field may name. "anthropic" is the
// default when the field is absent.
inline constexpr std::string_view kProviderAnthropic = "anthropic";
inline constexpr std::string_view kProviderOpenAIChat = "openai-chat";
inline constexpr std::string_view kProviderBedrock = "bedrock";
inline constexpr std::string_view kProviderVertex = "vertex";
const std::vector<std::string>& providerKinds();
bool isProviderKind(std::string_view kind);
// Throws std::invalid_argument naming the allowed kinds.
void validateProviderKind(const std::string& kind);
// Request path used when messages_path is absent: "/v1/chat/completions" for
// openai-chat, "/v1/messages" otherwise.
std::string defaultMessagesPath(std::string_view kind);
// The "betas" field: names sent in anthropic-beta (Bedrock: the anthropic_beta
// body field). Throws std::invalid_argument unless every entry is a nonempty
// string of visible ASCII without commas.
std::vector<std::string> parseBetas(const Json& betas);
void validateBetas(const std::vector<std::string>& betas);
// openai-chat has no beta header, so its betas are never sent.
bool kindSendsBetas(std::string_view kind);

// OpenAI Chat Completions differences between servers ("quirks" in
// native-client.json). Absent keys keep these defaults.
struct ChatQuirks {
  std::string maxTokensField = "max_tokens"; // max_tokens | max_completion_tokens
  std::string reasoning = "none";            // none | openai | reasoning_content | openrouter
  std::string toolIds = "any";               // any | short
  bool usageInStream = true;
  std::string systemRole = "system";         // system | developer
};
// Throws std::invalid_argument for unknown keys or values.
void validateQuirks(const Json& quirks);
ChatQuirks chatQuirks(const Json& quirks);

// GET URL for the model list: base_url + models_path, or /v1/models
// (/models when base_url already ends in /v1).
std::string modelsUrl(const std::string& baseUrl, const std::string& modelsPath);

using OnEvent = std::function<void(const Json&)>;
using OnResponse = std::function<void(const Response&)>;
using Timeout = std::optional<std::chrono::milliseconds>;

// One request seam for every provider. Callers always pass an Anthropic
// Messages body and always receive Anthropic-shaped results: stream events
// through onEvent, a Messages response body when stream is false, and
// APIError bodies of the form {"type":"error","error":{"type","message"}}.
class WireClient {
public:
  virtual ~WireClient() = default;
  virtual Response request(const Json& anthropicBody, bool stream,
                           const OnEvent& onEvent = {},
                           std::stop_token stop = {},
                           const OnResponse& onResponse = {},
                           Timeout timeout = std::nullopt) = 0;
  // {"data":[{"id":...},...]}
  virtual Json listModels() = 0;
  virtual std::string kind() = 0;
};

// Anthropic Messages servers: the existing transport::Client, unchanged.
class AnthropicMessagesClient final : public WireClient {
public:
  explicit AnthropicMessagesClient(ClientConfig config);
  Response request(const Json& anthropicBody, bool stream,
                   const OnEvent& onEvent = {},
                   std::stop_token stop = {},
                   const OnResponse& onResponse = {},
                   Timeout timeout = std::nullopt) override;
  Json listModels() override;
  std::string kind() override;
private:
  ClientConfig config_;
  Client client_;
};

} // namespace batbox::transport
