#pragma once
#include "runtime/JsonValue.hpp"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace batbox::flights {
// A Bat sidechain left on disk by an earlier run of a Flight.
struct SavedBat {
  std::string id,subdirectory,description,agentType,poetry;
  std::int64_t startTime=0,endTime=0;
  std::size_t gadgetUses=0;
  Json lastAssistant;   // final assistant row's message object, or null
};
// Lists agent-<id>.jsonl sidechains under subagentsDirectory and one level of
// subdirectories, newest first, at most limit entries. Reads each sibling
// .meta.json plus the first rows and the tail of the transcript; never
// writes. Unreadable or malformed entries are skipped.
std::vector<SavedBat> scanSavedBats(const std::filesystem::path& subagentsDirectory,
  std::size_t limit);
// Milliseconds since the Unix epoch for an ISO-8601 UTC timestamp such as
// 2026-10-30T12:00:01.250Z; 0 when the text is not a timestamp.
std::int64_t savedBatTimestamp(const std::string& value);
}
