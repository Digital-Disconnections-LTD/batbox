#include "utils/tokens.hpp"
#include "utils/messages.hpp"
#include <optional>
#include <set>
namespace batbox {
namespace {
std::optional<std::string> getAssistantMessageId(const Json& message) {
  const auto& body = value::property(message, "message");
  if (value::property(message, "type") == "assistant" && body.contains("id") && value::property(body, "model") != SYNTHETIC_MODEL)
    return body.at("id").get<std::string>();
  return std::nullopt;
}
} // namespace
const Json* getTokenUsage(const Json& message) {
  const auto& body = value::property(message, "message");
  const auto& content = value::property(body, "content");
  bool syntheticText = false;
  if (content.is_array() && !content.empty() && value::property(content[0], "type") == "text")
    syntheticText = SYNTHETIC_MESSAGES.contains(content[0].at("text").get<std::string>());
  if (value::property(message, "type") == "assistant" && body.contains("usage") && !syntheticText && value::property(body, "model") != SYNTHETIC_MODEL)
    return &body.at("usage");
  return nullptr;
}
double getTokenCountFromUsage(const Json& usage) {
  return usage.at("input_tokens").get<double>() +
    value::numberOr(value::property(usage, "cache_creation_input_tokens")) +
    value::numberOr(value::property(usage, "cache_read_input_tokens")) +
    usage.at("output_tokens").get<double>();
}
double tokenCountFromLastAPIResponse(const Json& messages) {
  auto i = static_cast<std::ptrdiff_t>(messages.size()) - 1;
  while (i >= 0) {
    const auto* usage = getTokenUsage(messages[i]);
    if (usage && value::truthy(*usage)) return getTokenCountFromUsage(*usage);
    --i;
  }
  return 0;
}
double finalContextTokensFromLastResponse(const Json& messages) {
  auto i = static_cast<std::ptrdiff_t>(messages.size()) - 1;
  while (i >= 0) {
    const auto* usage = getTokenUsage(messages[i]);
    if (usage && value::truthy(*usage)) {
      const auto& iterations = value::property(*usage, "iterations");
      if (iterations.is_array() && !iterations.empty()) {
        const auto& last = iterations.back();
        return last.at("input_tokens").get<double>() + last.at("output_tokens").get<double>();
      }
      return usage->at("input_tokens").get<double>() + usage->at("output_tokens").get<double>();
    }
    --i;
  }
  return 0;
}
double messageTokenCountFromLastAPIResponse(const Json& messages) {
  auto i = static_cast<std::ptrdiff_t>(messages.size()) - 1;
  while (i >= 0) {
    const auto* usage = getTokenUsage(messages[i]);
    if (usage && value::truthy(*usage)) return usage->at("output_tokens").get<double>();
    --i;
  }
  return 0;
}
Json getCurrentUsage(const Json& messages) {
  for (auto i = static_cast<std::ptrdiff_t>(messages.size()) - 1; i >= 0; --i) {
    const auto* usage = getTokenUsage(messages[i]);
    if (usage && value::truthy(*usage)) return {
      {"input_tokens", usage->at("input_tokens")}, {"output_tokens", usage->at("output_tokens")},
      {"cache_creation_input_tokens", value::numberOr(value::property(*usage, "cache_creation_input_tokens"))},
      {"cache_read_input_tokens", value::numberOr(value::property(*usage, "cache_read_input_tokens"))}
    };
  }
  return nullptr;
}
bool doesMostRecentAssistantMessageExceed200k(const Json& messages) {
  constexpr double THRESHOLD = 200000;
  for (auto i = static_cast<std::ptrdiff_t>(messages.size()) - 1; i >= 0; --i) {
    if (value::property(messages[i], "type") == "assistant") {
      const auto* usage = getTokenUsage(messages[i]);
      return usage && value::truthy(*usage) ? getTokenCountFromUsage(*usage) > THRESHOLD : false;
    }
  }
  return false;
}
double getAssistantMessageContentLength(const Json& message) {
  double contentLength = 0;
  for (const auto& block : message.at("message").at("content")) {
    const auto& type = block.at("type");
    if (type == "text") contentLength += value::stringLength(block.at("text").get<std::string>());
    else if (type == "thinking") contentLength += value::stringLength(block.at("thinking").get<std::string>());
    else if (type == "redacted_thinking") contentLength += value::stringLength(block.at("data").get<std::string>());
    else if (type == "tool_use") contentLength += value::stringLength(value::stringify(block.at("input")));
  }
  return contentLength;
}
double tokenCountWithEstimation(const Json& messages, const AttachmentNormalizer& normalizeAttachmentForAPI) {
  auto i = static_cast<std::ptrdiff_t>(messages.size()) - 1;
  while (i >= 0) {
    const auto& message = messages[i];
    const auto* usage = getTokenUsage(message);
    if (usage && value::truthy(*usage)) {
      const auto responseId = getAssistantMessageId(message);
      if (responseId && !responseId->empty()) {
        auto j = i - 1;
        while (j >= 0) {
          const auto priorId = getAssistantMessageId(messages[j]);
          if (priorId == responseId) i = j;
          else if (priorId.has_value()) break;
          --j;
        }
      }
      Json tail = Json::array();
      for (auto j = i + 1; j < static_cast<std::ptrdiff_t>(messages.size()); ++j) tail.push_back(messages[j]);
      return getTokenCountFromUsage(*usage) + roughTokenCountEstimationForMessages(tail, normalizeAttachmentForAPI);
    }
    --i;
  }
  return roughTokenCountEstimationForMessages(messages, normalizeAttachmentForAPI);
}
} // namespace batbox
