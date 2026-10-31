#include "services/tokenEstimation.hpp"
namespace batbox {
double roughTokenCountEstimation(const std::string& content, double bytesPerToken) {
  return value::round(static_cast<double>(value::stringLength(content)) / bytesPerToken);
}
double bytesPerTokenForFileType(const std::string& fileExtension) {
  if (fileExtension == "json" || fileExtension == "jsonl" || fileExtension == "jsonc") return 2;
  return 4;
}
double roughTokenCountEstimationForFileType(const std::string& content, const std::string& fileExtension) {
  return roughTokenCountEstimation(content, bytesPerTokenForFileType(fileExtension));
}
double roughTokenCountEstimationForMessages(const Json& messages, const AttachmentNormalizer& normalizeAttachmentForAPI) {
  double totalTokens = 0;
  for (const auto& message : messages) totalTokens += roughTokenCountEstimationForMessage(message, normalizeAttachmentForAPI);
  return totalTokens;
}
double roughTokenCountEstimationForMessage(const Json& message, const AttachmentNormalizer& normalizeAttachmentForAPI) {
  const auto& type = value::property(message, "type");
  const auto& content = value::property(value::property(message, "message"), "content");
  if ((type == "assistant" || type == "user") && value::truthy(content)) return roughTokenCountEstimationForContent(content);
  const auto& attachment = value::property(message, "attachment");
  if (type == "attachment" && value::truthy(attachment)) {
    const auto userMessages = normalizeAttachmentForAPI(attachment);
    double total = 0;
    for (const auto& userMsg : userMessages) total += roughTokenCountEstimationForContent(userMsg.at("message").at("content"));
    return total;
  }
  return 0;
}
double roughTokenCountEstimationForContent(const Json& content) {
  if (!value::truthy(content)) return 0;
  if (content.is_string()) return roughTokenCountEstimation(content.get<std::string>());
  double totalTokens = 0;
  for (const auto& block : content) totalTokens += roughTokenCountEstimationForBlock(block);
  return totalTokens;
}
double roughTokenCountEstimationForBlock(const Json& block) {
  if (block.is_string()) return roughTokenCountEstimation(block.get<std::string>());
  const auto& type = block.at("type");
  if (type == "text") return roughTokenCountEstimation(block.at("text").get<std::string>());
  if (type == "image" || type == "document") return 2000;
  if (type == "tool_result") return roughTokenCountEstimationForContent(value::property(block, "content"));
  if (type == "tool_use") {
    auto input = value::property(block, "input");
    if (input.is_null()) input = Json::object();
    return roughTokenCountEstimation(block.at("name").get<std::string>() + value::stringify(input));
  }
  if (type == "thinking") return roughTokenCountEstimation(block.at("thinking").get<std::string>());
  if (type == "redacted_thinking") return roughTokenCountEstimation(block.at("data").get<std::string>());
  return roughTokenCountEstimation(value::stringify(block));
}
} // namespace batbox
