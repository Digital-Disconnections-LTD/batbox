#include "interactive/MessageCollapse.hpp"
#include "interactive/MessageTags.hpp"
#include <stdexcept>

namespace batbox::interactive {
namespace {
const Json* member(const Json& object, const char* key) {
  if (!object.is_object()) return nullptr;
  const auto found = object.find(key);
  return found == object.end() ? nullptr : &*found;
}
std::string value(const Json& object, const char* key) {
  const auto* field = member(object, key);
  return field && field->is_string() ? field->get<std::string>() : "";
}
bool completedBash(const RenderItem& item,
                   const BackgroundNotificationProtocol& protocol) {
  if (!item.message || value(*item.message, "type") != "user") return false;
  const auto* inner = member(*item.message, "message");
  const auto* content = inner ? member(*inner, "content") : nullptr;
  if (!content || !content->is_array() || content->empty()) return false;
  const auto& block = (*content)[0];
  if (value(block, "type") != "text") return false;
  const auto text = value(block, "text");
  if (text.find("<" + protocol.missionTag) == std::string::npos) return false;
  if (extractTag(text, protocol.statusTag) != "completed") return false;
  const auto summary = extractTag(text, protocol.summaryTag);
  return summary && summary->starts_with(protocol.bashSummaryPrefix);
}
} // namespace

RenderItemsRef collapseBackgroundBashNotifications(
    const RenderItemsRef& messages, bool verbose, bool fullscreen,
    const BackgroundNotificationProtocol& protocol) {
  if (!messages) throw std::invalid_argument("Message list required");
  if (!fullscreen || verbose) return messages;
  auto result = std::make_shared<RenderItems>();
  for (std::size_t i = 0; i < messages->size();) {
    const auto& item = (*messages)[i];
    if (!completedBash(item, protocol)) { result->push_back(item); ++i; continue; }
    const auto start = i;
    while (i < messages->size() && completedBash((*messages)[i], protocol)) ++i;
    const auto count = i - start;
    if (count == 1) { result->push_back(item); continue; }
    Json merged = *item.message;
    const auto message = "<" + protocol.missionTag + "><" + protocol.statusTag +
      ">completed</" + protocol.statusTag + "><" + protocol.summaryTag +
      ">" + std::to_string(count) + " background commands completed</" +
      protocol.summaryTag + "></" + protocol.missionTag + ">";
    merged["message"] = Json{{"role", "user"},
      {"content", Json::array({Json{{"type", "text"}, {"text", message}}})}};
    result->push_back({std::make_shared<Json>(std::move(merged)), {}, {}, {}});
  }
  return result;
}
} // namespace batbox::interactive
