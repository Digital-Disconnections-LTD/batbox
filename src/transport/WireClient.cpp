#include "transport/WireClient.hpp"
#include "transport/RawHttp.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace batbox::transport {
namespace {
std::string joined(const std::vector<std::string>& values) {
  std::string text;
  for (std::size_t i = 0; i < values.size(); ++i) { if (i) text += ", "; text += values[i]; }
  return text;
}
std::string choice(const Json& quirks, const char* key, const std::vector<std::string>& allowed,
                   const std::string& fallback) {
  if (!quirks.contains(key)) return fallback;
  const auto& value = quirks.at(key);
  if (!value.is_string() || std::find(allowed.begin(),allowed.end(),value.get<std::string>()) == allowed.end())
    throw std::invalid_argument(std::string("quirks.") + key + " must be one of: " + joined(allowed));
  return value.get<std::string>();
}
const std::vector<std::string> kMaxTokensFields{"max_tokens","max_completion_tokens"};
const std::vector<std::string> kReasoning{"none","openai","reasoning_content","openrouter"};
const std::vector<std::string> kToolIds{"any","short"};
const std::vector<std::string> kSystemRoles{"system","developer"};
const std::vector<std::string> kQuirkKeys{"max_tokens_field","reasoning","tool_ids","usage_in_stream","system_role"};
} // namespace

const std::vector<std::string>& providerKinds() {
  static const std::vector<std::string> kinds{std::string(kProviderAnthropic),std::string(kProviderOpenAIChat),
    std::string(kProviderBedrock),std::string(kProviderVertex)};
  return kinds;
}
bool isProviderKind(std::string_view kind) {
  const auto& kinds = providerKinds();
  return std::find(kinds.begin(),kinds.end(),kind) != kinds.end();
}
void validateProviderKind(const std::string& kind) {
  if (!isProviderKind(kind))
    throw std::invalid_argument("Unknown provider \"" + kind + "\"; use one of: " + joined(providerKinds()));
}
std::string defaultMessagesPath(std::string_view kind) {
  return kind == kProviderOpenAIChat ? "/v1/chat/completions" : "/v1/messages";
}

void validateQuirks(const Json& quirks) { (void)chatQuirks(quirks); }
ChatQuirks chatQuirks(const Json& quirks) {
  ChatQuirks result;
  if (quirks.is_null()) return result;
  if (!quirks.is_object()) throw std::invalid_argument("quirks must be a JSON object");
  for (auto it = quirks.begin(); it != quirks.end(); ++it)
    if (std::find(kQuirkKeys.begin(),kQuirkKeys.end(),it.key()) == kQuirkKeys.end())
      throw std::invalid_argument("Unknown quirk \"" + it.key() + "\"; use: " + joined(kQuirkKeys));
  result.maxTokensField = choice(quirks,"max_tokens_field",kMaxTokensFields,result.maxTokensField);
  result.reasoning = choice(quirks,"reasoning",kReasoning,result.reasoning);
  result.toolIds = choice(quirks,"tool_ids",kToolIds,result.toolIds);
  result.systemRole = choice(quirks,"system_role",kSystemRoles,result.systemRole);
  if (quirks.contains("usage_in_stream")) {
    if (!quirks.at("usage_in_stream").is_boolean())
      throw std::invalid_argument("quirks.usage_in_stream must be true or false");
    result.usageInStream = quirks.at("usage_in_stream").get<bool>();
  }
  return result;
}

std::string modelsUrl(const std::string& baseUrl, const std::string& modelsPath) {
  auto base = baseUrl;
  while (base.ends_with('/')) base.pop_back();
  if (!modelsPath.empty()) {
    if (modelsPath.front() != '/') throw std::invalid_argument("Models path must begin with /");
    if (base.ends_with("/v1") && modelsPath.starts_with("/v1/")) base.resize(base.size()-3);
    return base + modelsPath;
  }
  return base.ends_with("/v1") ? base + "/models" : base + "/v1/models";
}

AnthropicMessagesClient::AnthropicMessagesClient(ClientConfig config)
  : config_(config), client_(std::move(config)) {}
Response AnthropicMessagesClient::request(const Json& anthropicBody, bool stream, const OnEvent& onEvent,
    std::stop_token stop, const OnResponse& onResponse, Timeout timeout) {
  try { return client_.request(anthropicBody,stream,onEvent,stop,onResponse,timeout); }
  catch (const APIConnectionError&) { throw; }
  catch (const APIError& error) {
    if (error.status && (*error.status == 401 || *error.status == 403))
      throw readableAuthError(error,config_.baseUrl);
    throw;
  }
}
Json AnthropicMessagesClient::listModels() {
  HttpSession session(config_);
  return fetchModelList(session,modelsUrl(config_.baseUrl,config_.modelsPath));
}
std::string AnthropicMessagesClient::kind() { return std::string(kProviderAnthropic); }
} // namespace batbox::transport
