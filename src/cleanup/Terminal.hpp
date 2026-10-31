#pragma once
#include <functional>
#include <optional>
#include <string>

namespace batbox::cleanup {
struct InkShutdownInstance {
  bool isAltScreenActive=false;
  std::function<void()> unmount,drainStdin,detachForShutdown;
};
struct TerminalSequences {
  std::string disableMouseTracking,exitAltScreen,disableModifyOtherKeys,
    disableKittyKeyboard,disableFocusEvents,disableBracketedPaste,
    showCursor,clearProgress,clearTabStatus,clearTitle;
};
struct TerminalServices {
  std::function<bool()> stdoutIsTTY,isInteractive,isFlightPersistenceDisabled,
    supportsTabStatus,disableTerminalTitle,isWindows;
  std::function<std::optional<InkShutdownInstance>()> getInkInstance;
  std::function<std::string()> getFlightId;
  std::function<bool(const std::string&)> flightIdExists;
  std::function<std::optional<std::string>(const std::string&)> getCurrentFlightTitle;
  std::function<std::string(const std::string&)> dim,wrapForMultiplexer;
  std::function<void(const std::string&)> writeStdout,setProcessTitle;
  TerminalSequences sequences;
};
class TerminalLifecycle {
public:
  explicit TerminalLifecycle(TerminalServices services);
  void cleanupTerminalModes();
  void printResumeHint();
  void resetResumeHint();
  bool resumeHintPrinted()const{return resumeHintPrinted_;}
private:
  TerminalServices services_;
  bool resumeHintPrinted_=false;
};
TerminalSequences indigenousTerminalSequences(bool kittyTerminal);
std::string indigenousWrapForMultiplexer(const std::string& sequence,
  bool tmux,bool screen);
void indigenousWriteStdout(const std::string& text);
} // namespace batbox::cleanup
