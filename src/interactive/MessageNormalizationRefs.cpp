#include "interactive/MessageNormalizationRefs.hpp"
#include <stdexcept>

namespace batbox::interactive {
namespace {
std::string type(const Json& value) {
  return value.is_object() ? value.value("type", "") : "";
}
std::size_t outputCount(const Json& message) {
  const auto kind = type(message);
  if (kind == "assistant")
    return message.at("message").at("content").size();
  if (kind == "user") {
    const auto& content = message.at("message").at("content");
    return content.is_string() ? 1 : content.size();
  }
  return 1;
}
} // namespace

MessageRefs normalizeMessagesRefs(const MessageRefs& messages,
                                   messages::Services& services) {
  Json input = Json::array();
  for (const auto& message : messages)
    input.push_back(message ? *message : Json(nullptr));
  const auto normalized = messages::normalizeMessages(input, services);
  MessageRefs result;
  result.reserve(normalized.size());
  std::size_t offset = 0;
  for (const auto& message : messages) {
    if (!message) throw std::invalid_argument("Message reference required");
    const auto kind = type(*message);
    const auto count = outputCount(*message);
    for (std::size_t i = 0; i < count; ++i) {
      if (offset >= normalized.size()) throw std::logic_error("Normalizer output mismatch");
      if (kind == "attachment" || kind == "progress" || kind == "system")
        result.push_back(message);
      else result.push_back(std::make_shared<Json>(normalized[offset]));
      ++offset;
    }
  }
  if (offset != normalized.size()) throw std::logic_error("Normalizer output mismatch");
  return result;
}
} // namespace batbox::interactive
