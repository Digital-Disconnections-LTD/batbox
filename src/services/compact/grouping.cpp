#include "services/compact/grouping.hpp"
namespace batbox {
Json groupMessagesByApiRound(const Json& messages) {
  Json groups = Json::array();
  Json current = Json::array();
  Json lastAssistantId;
  for (const auto& msg : messages) {
    const auto& id = value::property(value::property(msg, "message"), "id");
    if (msg.at("type") == "assistant" && id != lastAssistantId && !current.empty()) {
      groups.push_back(current);
      current = Json::array({msg});
    } else current.push_back(msg);
    if (msg.at("type") == "assistant") lastAssistantId = id;
  }
  if (!current.empty()) groups.push_back(current);
  return groups;
}
} // namespace batbox
