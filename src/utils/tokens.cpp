#include "utils/tokens.hpp"
#include "utils/messages.hpp"
#include <array>
#include <functional>
#include <mutex>
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
namespace {
// A response always reads at least one input token; zero input means the
// server sent no usage and the reducer kept its empty default.
bool reportsInput(const Json& usage) {
  return value::numberOr(value::property(usage, "input_tokens")) +
    value::numberOr(value::property(usage, "cache_creation_input_tokens")) +
    value::numberOr(value::property(usage, "cache_read_input_tokens")) > 0;
}
std::optional<std::string> uuidOf(const Json& message) {
  const auto& uuid = value::property(message, "uuid");
  if (uuid.is_string() && !uuid.get<std::string>().empty()) return uuid.get<std::string>();
  return std::nullopt;
}
// Estimates for whole conversations without usage, so each request only
// estimates the messages added since the last one. Keyed by the first
// message, which identifies the main thread and each Bat.
struct Estimate { std::string first; std::size_t count = 0, ids = 0; double tokens = 0; };
// Message identities of the first count messages; cheap next to estimating them.
std::size_t identities(const Json& messages, std::size_t count) {
  std::size_t hash = 1469598103934665603ull;
  for (std::size_t index = 0; index < count; ++index) {
    const auto id = uuidOf(messages[index]);
    hash = (hash ^ std::hash<std::string>{}(id.value_or("-"))) * 1099511628211ull;
  }
  return hash;
}
std::mutex estimatesMutex;
std::array<Estimate, 8> estimates;
std::size_t nextEstimate = 0;
double estimateAll(const Json& messages, const AttachmentNormalizer& normalize) {
  const auto first = messages.empty() ? std::nullopt : uuidOf(messages.front());
  if (!first) return roughTokenCountEstimationForMessages(messages, normalize);
  std::size_t start = 0;
  double tokens = 0;
  std::optional<Estimate> known;
  {
    std::lock_guard lock(estimatesMutex);
    for (const auto& entry : estimates)
      if (entry.count && entry.first == *first && entry.count <= messages.size()) {known = entry;break;}
  }
  if (known && identities(messages, known->count) == known->ids) {
    start = known->count;tokens = known->tokens;
  }
  for (auto index = start; index < messages.size(); ++index)
    tokens += roughTokenCountEstimationForMessage(messages[index], normalize);
  std::lock_guard lock(estimatesMutex);
  auto* slot = &estimates[nextEstimate];
  for (auto& entry : estimates) if (entry.first == *first) { slot = &entry; break; }
  if (slot == &estimates[nextEstimate]) nextEstimate = (nextEstimate + 1) % estimates.size();
  *slot = Estimate{*first, messages.size(), identities(messages, messages.size()), tokens};
  return tokens;
}
} // namespace
double tokenCountWithEstimation(const Json& messages, const AttachmentNormalizer& normalizeAttachmentForAPI) {
  auto i = static_cast<std::ptrdiff_t>(messages.size()) - 1;
  while (i >= 0) {
    const auto& message = messages[i];
    const auto* usage = getTokenUsage(message);
    if (usage && value::truthy(*usage) && reportsInput(*usage)) {
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
      double tail = 0;
      for (auto j = i + 1; j < static_cast<std::ptrdiff_t>(messages.size()); ++j)
        tail += roughTokenCountEstimationForMessage(messages[j], normalizeAttachmentForAPI);
      return getTokenCountFromUsage(*usage) + tail;
    }
    --i;
  }
  return estimateAll(messages, normalizeAttachmentForAPI);
}
} // namespace batbox
