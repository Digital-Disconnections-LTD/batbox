#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace batbox::terminal_hooks {
using WriteRaw=std::function<void(const std::string&)>;
using WriteRawRef=std::shared_ptr<WriteRaw>;
struct OscServices {
  std::function<std::string(const Json&)> osc;
  std::function<std::string(const std::string&)> wrapForMultiplexer;
};
enum class ProgressState {Running,Completed,Error,Indeterminate};
struct NotificationServices:OscServices {
  std::function<bool()> isProgressReportingAvailable;
};
class TerminalNotification {
public:
  TerminalNotification(WriteRawRef writeRaw,NotificationServices services);
  void notifyITerm2(const std::string& message,
    const std::optional<std::string>& title={})const;
  void notifyKitty(const std::string& message,const std::string& title,
    double id)const;
  void notifyGhostty(const std::string& message,
    const std::string& title)const;
  void notifyBell()const;
  void progress(std::optional<ProgressState> state,
    std::optional<double> percentage={})const;
private:
  WriteRawRef writeRaw_;
  NotificationServices services_;
  void writeWrapped(const Json& parts)const;
};
// The owning React commit adapter calls render() at hook render time. The
// returned object is stable when TerminalWriteContext's function identity is.
class NotificationHook {
public:
  std::shared_ptr<TerminalNotification> render(
    WriteRawRef writeRaw,const NotificationServices& services);
private:
  WriteRawRef previousWrite_;
  std::shared_ptr<TerminalNotification> memo_;
};
}
