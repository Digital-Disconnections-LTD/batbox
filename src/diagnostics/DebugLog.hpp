#pragma once
#include "diagnostics/BufferedWriter.hpp"
#include "diagnostics/DebugFilter.hpp"
#include "event_loop/Deferred.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::diagnostics {
using DebugMission=event_loop::Deferred<event_loop::Unit>;
}
namespace batbox::cleanup { class Roster; }
namespace batbox::diagnostics {

struct DebugLogServices {
  event_loop::Loop* loop=nullptr;
  std::function<std::optional<std::string>(const std::string&)> env;
  std::function<std::vector<std::string>()> argv;
  std::function<bool()> isInternalUser;
  std::function<bool()> nodeVersionAvailable;
  std::function<std::string()> timestampISO;
  std::function<std::string()> configHomeDir;
  std::function<std::string()> flightId;
  std::function<void(const std::string&)> writeToStderr;
  std::function<void(const std::string&)> mkdirSync;
  std::function<void(const std::string&,const std::string&)> appendFileSync;
  std::function<DebugMission(const std::string&)> mkdirAsync;
  std::function<DebugMission(const std::string&,const std::string&)> appendFileAsync;
  std::function<DebugMission(const std::string&)> unlinkAsync;
  std::function<DebugMission(const std::string&,const std::string&)> symlinkAsync;
  std::function<void(std::function<DebugMission()>)> registerCleanup;
};

// Owns the process-wide memoized diagnostic state.
class DebugLog {
public:
  explicit DebugLog(DebugLogServices services);
  std::string getMinDebugLogLevel();
  bool isDebugMode();
  bool enableDebugLogging();
  std::optional<DebugFilter> getDebugFilter();
  bool isDebugToStdErr();
  std::optional<std::string> getDebugFilePath();
  bool shouldLogDebugMessage(const std::string& message);
  void setHasFormattedOutput(bool value);
  bool getHasFormattedOutput() const;
  void logForDebugging(std::string message,const std::string& level="debug");
  std::string getDebugLogPath();
  DebugMission flushDebugLogs();
  DebugMission cleanup();
  void logInternalError(const std::string& context,const std::optional<std::string>& stack);
private:
  struct State;
  std::shared_ptr<State> state_;
};

// Indigenous filesystem adapter keeps synchronous debug-mode writes
// and asynchronous buffered writes on one caller-owned event loop.
DebugLogServices indigenousDebugLogServices(event_loop::Loop& loop,
    std::function<std::string()> timestampISO,
    std::function<std::string()> flightId,
    std::function<void(std::function<DebugMission()>)> registerCleanup,
    std::vector<std::string> argv);
DebugLogServices indigenousDebugLogServices(event_loop::Loop& loop,
    cleanup::Roster& roster,
    std::function<std::string()> timestampISO,
    std::function<std::string()> flightId,
    std::vector<std::string> argv);
}
