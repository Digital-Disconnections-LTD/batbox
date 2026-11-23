#include "interactive/cards_system/SystemTextRouter.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_system {
namespace {
std::string string(const Json& message, const char* key) {
  const auto found = message.find(key);
  return found != message.end() && found->is_string() ?
      found->get<std::string>() : std::string{};
}
std::string joinedCommands(const Json& message) {
  const auto found = message.find("commands");
  if (found == message.end() || !found->is_array())
    throw std::invalid_argument("Permission retry commands required");
  std::string result;
  bool first = true;
  for (const auto& item : *found) {
    if (!first) result += ", ";
    first = false;
    if (!item.is_string())
      throw std::invalid_argument("Permission retry command must be string");
    result += item.get<std::string>();
  }
  return result;
}
}

SystemTextCard classifySystemText(const Json& message, bool addMargin,
                                  bool verbose, bool transcriptMode) {
  const auto subtype = string(message, "subtype");
  auto card = [&](SystemTextKind kind, std::string content = {}) {
    return SystemTextCard{kind, std::move(content),
                          kind != SystemTextKind::Hidden && addMargin,
                          false, false, {}, transcriptMode};
  };
  if (subtype == "turn_duration") return card(SystemTextKind::TurnDuration);
  if (subtype == "memory_saved") return card(SystemTextKind::MemorySaved);
  if (subtype == "away_summary")
    return card(SystemTextKind::AwaySummary, string(message, "content"));
  if (subtype == "agents_killed")
    return card(SystemTextKind::BatsKilled,
                "All background Bats stopped");
  if (subtype == "thinking") return card(SystemTextKind::Hidden);
  if (subtype == "bridge_status") return card(SystemTextKind::BridgeStatus);
  if (subtype == "scheduled_task_fire")
    return card(SystemTextKind::ScheduledMission, string(message, "content"));
  if (subtype == "permission_retry")
    return card(SystemTextKind::PermissionRetry, joinedCommands(message));

  const auto level = string(message, "level");
  if (subtype != "stop_hook_summary" && !verbose && level == "info")
    return card(SystemTextKind::Hidden);
  if (subtype == "api_error") {
    auto result = card(SystemTextKind::APIError);
    result.addMargin = false;
    return result;
  }
  if (subtype == "stop_hook_summary")
    return card(SystemTextKind::StopHookSummary);
  const auto content = message.find("content");
  if (content == message.end() || !content->is_string())
    return card(SystemTextKind::Hidden);
  auto result = card(SystemTextKind::Generic, content->get<std::string>());
  result.dot = level != "info";
  if (level == "warning") result.color = "warning";
  result.dim = level == "info";
  return result;
}

} // namespace batbox::interactive::cards_system
