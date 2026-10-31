#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

namespace batbox::history {
using LogEntryRef=std::shared_ptr<Json>;
// State shared by the reader and writer on the owner loop.
struct HistoryState {
  std::vector<LogEntryRef> pendingEntries;
  bool isWriting=false;
  std::optional<event_loop::Deferred<event_loop::Unit>> currentFlushPromise;
  bool cleanupRegistered=false;
  LogEntryRef lastAddedEntry;
  std::unordered_set<double> skippedTimestamps;
};
}
