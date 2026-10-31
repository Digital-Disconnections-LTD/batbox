#pragma once

#include "runtime/JsonValue.hpp"
#include <optional>
#include <string>

namespace batbox::interactive::cards_system {

enum class SystemTextKind {
  Hidden, TurnDuration, MemorySaved, AwaySummary, BatsKilled,
  BridgeStatus, ScheduledMission, PermissionRetry, APIError,
  StopHookSummary, Generic
};

struct SystemTextCard {
  SystemTextKind kind = SystemTextKind::Hidden;
  std::string content;
  bool addMargin = false;
  bool dot = false;
  bool dim = false;
  std::optional<std::string> color;
  bool transcriptMode = false;
};

SystemTextCard classifySystemText(const Json& message, bool addMargin,
                                  bool verbose, bool transcriptMode);
const char* systemTextKindName(SystemTextKind kind);

} // namespace batbox::interactive::cards_system
