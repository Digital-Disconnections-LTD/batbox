#pragma once
#include "history/Reader.hpp"
#include "history/PastedContent.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace batbox::history {
struct LogEntryCursor {
  std::function<event_loop::Deferred<std::optional<LogEntryRef>>()> next;
  std::function<event_loop::Deferred<event_loop::Unit>()> close;
};
struct HistoryViewImports {
  event_loop::Loop* loop=nullptr;
  std::function<LogEntryCursor()> makeLogEntryReader;
  std::function<event_loop::Deferred<Json>(const LogEntryRef&)>
    logEntryToHistoryEntry;
  std::function<std::string()> getProjectRoot,getFlightId;
};
class HistorySimpleCursor {
public:
  struct State;
  explicit HistorySimpleCursor(HistoryViewImports imports);
  event_loop::Deferred<std::optional<Json>> next();
  event_loop::Deferred<event_loop::Unit> close();
private:
  std::shared_ptr<State> state_;
};
HistoryViewImports indigenousHistoryViewImports(event_loop::Loop& loop,
  std::shared_ptr<HistoryState> shared,HistoryReaderImports reader,
  PastedContentImports pastes,
  std::function<std::string()> getProjectRoot,
  std::function<std::string()> getFlightId);
}
