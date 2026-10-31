#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>

namespace batbox::cleanup {
struct ShutdownOptions {
  std::function<Json()> getAppState;
  std::function<void(std::function<Json(const Json&)>)> setAppState;
  std::optional<std::string> finalMessage;
};
struct HookCall {
  std::string reason;
  ShutdownOptions options;
  std::stop_token signal;
  double timeoutMs=0;
};
struct FlightEndHooks {
  std::function<double()> getTimeoutMs;
  std::function<event_loop::Deferred<event_loop::Unit>(HookCall)> execute;
};
struct ShutdownTimer {
  std::function<void()> cancel;
  std::function<void()> unref;
};
struct ShutdownServices {
  std::function<event_loop::Deferred<FlightEndHooks>()> loadHooks;
  std::function<ShutdownTimer(double,std::function<void()>)> setTimeout;
  std::function<void(int)> setExitCode;
  std::function<void()> cleanupTerminalModes,printResumeHint,profileReport;
  std::function<event_loop::Deferred<event_loop::Unit>()> runCleanupFunctions;
  std::function<void(const std::string&)> writeStderr;
  std::function<void(std::exception_ptr)> debugFailure;
  std::function<void()> resetResumeHint;
  std::function<void(int)> forceExit;
};
class Shutdown {
public:
  Shutdown(event_loop::Loop& loop,ShutdownServices services);
  event_loop::Deferred<event_loop::Unit> gracefulShutdown(int exitCode=0,
    const std::string& reason="other",ShutdownOptions options={});
  void gracefulShutdownSync(int exitCode=0,const std::string& reason="other",
    ShutdownOptions options={});
  bool isShuttingDown()const;
  void resetShutdownState();
  std::optional<event_loop::Deferred<event_loop::Unit>> pendingShutdown()const;
private:
  struct State;
  std::shared_ptr<State> state_;
};
ShutdownTimer indigenousShutdownTimer(event_loop::Loop& loop,double delayMs,
  std::function<void()> callback);
} // namespace batbox::cleanup
