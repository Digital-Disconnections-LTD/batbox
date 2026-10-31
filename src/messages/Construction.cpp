#include "messages/Internal.hpp"
#include <iomanip>
#include <sstream>

namespace batbox::messages {
using namespace internal;
namespace {
Json baseCreateAssistantMessage(const Json& args, Services& services) {
  Json usage = get(args, "usage");
  if (!has(args, "usage")) {
    usage = Json{{"input_tokens", 0}, {"output_tokens", 0},
      {"cache_creation_input_tokens", 0}, {"cache_read_input_tokens", 0},
      {"server_tool_use", Json{{"web_search_requests", 0}, {"web_fetch_requests", 0}}},
      {"service_tier", nullptr},
      {"cache_creation", Json{{"ephemeral_1h_input_tokens", 0}, {"ephemeral_5m_input_tokens", 0}}},
      {"inference_geo", nullptr}, {"iterations", nullptr}, {"speed", nullptr}};
  }
  Json result = {{"type", "assistant"}, {"uuid", requireUUID(services)}, {"timestamp", requireNow(services)}};
  result["message"] = Json{{"id", requireUUID(services)}, {"container", nullptr},
    {"model", SYNTHETIC_MODEL}, {"role", "assistant"}, {"stop_reason", "stop_sequence"},
    {"stop_sequence", ""}, {"type", "message"}, {"usage", usage},
    {"content", args.at("content")}, {"context_management", nullptr}};
  copyIf(result, args, "apiError");
  copyIf(result, args, "error");
  copyIf(result, args, "errorDetails");
  result["isApiErrorMessage"] = has(args, "isApiErrorMessage") ? args.at("isApiErrorMessage") : Json(false);
  copyIf(result, args, "isVirtual");
  return result;
}
} // namespace

std::string deriveUUID(const std::string& parentUUID, std::size_t index) {
  std::ostringstream hex; hex << std::hex << std::setfill('0') << std::setw(12) << index;
  return parentUUID.substr(0, 24) + hex.str();
}
Json createUserMessage(const Json& args, Services& services) {
  Json result = {{"type", "user"}};
  const Json content = get(args, "content");
  result["message"] = Json{{"role", "user"}, {"content", value::truthy(content) ? content : Json(NO_CONTENT_MESSAGE)}};
  for (const auto* key : {"isMeta", "isVisibleInTranscriptOnly", "isVirtual", "isCompactSummary", "summarizeMetadata"}) copyIf(result, args, key);
  result["uuid"] = value::truthy(get(args, "uuid")) ? get(args, "uuid") : Json(requireUUID(services));
  result["timestamp"] = has(args, "timestamp") && !get(args, "timestamp").is_null() ? get(args, "timestamp") : Json(requireNow(services));
  for (const auto* key : {"toolUseResult", "mcpMeta", "imagePasteIds", "sourceToolAssistantUUID", "permissionMode", "origin"}) copyIf(result, args, key);
  return result;
}
Json createAssistantMessage(const Json& args, Services& services) {
  Json base = Json::object();
  const Json content = args.at("content");
  if (content.is_string()) {
    std::string text = str(content);
    base["content"] = Json::array({textBlock(text.empty() ? NO_CONTENT_MESSAGE : text)});
  } else base["content"] = content;
  copyIf(base, args, "usage"); copyIf(base, args, "isVirtual");
  return baseCreateAssistantMessage(base, services);
}
Json createAssistantAPIErrorMessage(const Json& args, Services& services) {
  Json base = Json::object();
  std::string content = field(args, "content");
  base["content"] = Json::array({textBlock(content.empty() ? NO_CONTENT_MESSAGE : content)});
  base["isApiErrorMessage"] = true;
  for (const auto* key : {"apiError", "error", "errorDetails"}) copyIf(base, args, key);
  return baseCreateAssistantMessage(base, services);
}
Json prepareUserContent(const Json& args) {
  const Json& blocks = args.at("precedingInputBlocks");
  if (blocks.empty()) return args.at("inputString");
  Json result = blocks; result.push_back(textBlock(field(args, "inputString"))); return result;
}
} // namespace batbox::messages
