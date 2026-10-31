#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::history {
struct PastedContentImports {
  event_loop::Loop* loop=nullptr;
  std::function<event_loop::Deferred<std::optional<std::string>>(
    const std::string&)> retrievePastedText;
};
event_loop::Deferred<std::optional<Json>> resolveStoredPastedContent(
  const Json& stored,const PastedContentImports& imports);
event_loop::Deferred<Json> logEntryToHistoryEntry(
  const Json& entry,const PastedContentImports& imports);
}
