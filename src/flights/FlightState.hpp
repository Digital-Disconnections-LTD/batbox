#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace batbox::flights {
// Shared Project state lets synchronous and owner-loop compositions reuse the
// same metadata stash.
struct FlightState {
  struct PendingEntry {
    Json value;
    std::function<Json()> reader;
  };
  std::optional<std::string> flightFile;
  std::vector<PendingEntry> pendingEntries;
  std::unordered_map<std::string,std::string> existingFlightFiles;
  std::optional<std::string> currentFlightTag,currentFlightTitle,currentFlightAiTitle;
  std::optional<std::string> currentFlightBatName,currentFlightBatColor;
  std::optional<std::string> currentFlightLastPoetry,currentFlightBatSetting,currentFlightMode;
  std::optional<Json> currentFlightWorktree,currentFlightPrNumber;
  std::optional<std::string> currentFlightPrUrl,currentFlightPrRepository;
};
} // namespace batbox::flights
