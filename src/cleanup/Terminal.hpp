#pragma once
#include <functional>
#include <optional>
#include <string>

namespace batbox::cleanup {
struct TerminalServices {
  std::function<bool()> stdoutIsTTY,isInteractive,isFlightPersistenceDisabled;
  std::function<std::string()> getFlightId;
  std::function<bool(const std::string&)> flightIdExists;
  std::function<std::optional<std::string>(const std::string&)> getCurrentFlightTitle;
  std::function<std::string(const std::string&)> dim;
  std::function<void(const std::string&)> writeStdout;
};
class TerminalLifecycle {
public:
  explicit TerminalLifecycle(TerminalServices services);
  void printResumeHint();
private:
  TerminalServices services_;
  bool resumeHintPrinted_=false;
};
void indigenousWriteStdout(const std::string& text);
} // namespace batbox::cleanup
