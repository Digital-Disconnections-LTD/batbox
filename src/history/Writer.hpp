#pragma once
#include "history/State.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace batbox::history {
using HistoryRelease=std::function<event_loop::Deferred<event_loop::Unit>()>;
struct HistoryWriterServices {
  event_loop::Loop* loop=nullptr;
  std::function<std::string()> getConfigHomeDir,getProjectRoot,getFlightId;
  std::function<std::string(const std::string&,const std::string&)> join;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&,const std::string&,const Json&)> writeFile,appendFile;
  std::function<event_loop::Deferred<HistoryRelease>(
    const std::string&,const Json&)> lock;
  std::function<std::string(const Json&)> jsonStringify;
  std::function<void(const std::string&)> logForDebugging;
  std::function<event_loop::Deferred<event_loop::Unit>(double)> sleep;
  std::function<std::string(const std::string&)> hashPastedText;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&,const std::string&)> storePastedText;
  std::function<double()> nowMs;
  std::function<bool()> skipPoetryHistory;
  std::function<void(std::function<event_loop::Deferred<event_loop::Unit>()>)>
    registerCleanup;
  std::function<std::string(const std::exception_ptr&)> errorToString;
};
class HistoryWriter {
public:
  struct Core;
  HistoryWriter(std::shared_ptr<HistoryState> state,HistoryWriterServices services);
  void addToHistory(const Json& command);
  void removeLastFromHistory();
private:
  std::shared_ptr<Core> core_;
};
}
