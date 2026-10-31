#pragma once
#include "cleanup/Shutdown.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace batbox::cleanup {
struct DiagnosticError {
  std::string name,message;
  std::optional<std::string> stack;
};
struct RejectionReason {
  std::optional<DiagnosticError> error;
  std::string stringValue;
};
struct SignalServices {
  std::function<void()> pinExitHook;
  std::function<void(int,std::function<void()>)> onSignal;
  std::function<void(std::function<void(DiagnosticError)>)> onUncaughtException;
  std::function<void(std::function<void(RejectionReason)>)> onUnhandledRejection;
  std::function<ShutdownTimer(double,std::function<void()>)> setInterval;
  std::function<bool()> isPrintMode,isWindows,stdinIsTTY,
    isScrollDraining,stdoutWritable,stdinReadable;
  std::function<void(const std::string&,const std::string&,Json)> localDiagnostic;
  std::function<void(int)> gracefulShutdown;
};
class SignalLifecycle {
public:
  explicit SignalLifecycle(SignalServices services);
  void setupGracefulShutdown();
  bool installed()const;
private:
  struct State;
  std::shared_ptr<State> state_;
};
ShutdownTimer indigenousUnrefInterval(event_loop::Loop& loop,double delayMs,
  std::function<void()> callback);
} // namespace batbox::cleanup
