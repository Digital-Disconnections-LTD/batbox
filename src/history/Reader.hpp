#pragma once
#include "history/State.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace batbox::history {
struct HistoryLineCursor {
  std::function<event_loop::Deferred<std::optional<std::string>>()> next;
  std::function<event_loop::Deferred<event_loop::Unit>()> close;
};
struct HistoryReaderImports {
  event_loop::Loop* loop=nullptr;
  std::function<std::string()> getFlightId,getConfigHomeDir;
  std::function<std::string(const std::string&,const std::string&)> join;
  std::function<HistoryLineCursor(const std::string&)> readLinesReverse;
  std::function<Json(const std::string&)> jsonParse;
  std::function<void(const std::string&)> logForDebugging;
  std::function<std::optional<std::string>(const std::exception_ptr&)> getErrnoCode;
  std::function<std::string(const std::exception_ptr&)> errorToString;
};
class HistoryLogReader {
public:
  struct State;
  HistoryLogReader(std::shared_ptr<HistoryState> shared,HistoryReaderImports imports);
  event_loop::Deferred<std::optional<LogEntryRef>> next();
  event_loop::Deferred<event_loop::Unit> close();
private:
  std::shared_ptr<State> state_;
};
}
