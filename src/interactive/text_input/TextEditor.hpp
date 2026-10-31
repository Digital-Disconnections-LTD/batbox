#pragma once

#include "event_loop/Loop.hpp"
#include "interactive/PresentationState.hpp"
#include "interactive/text_input/Cursor.hpp"
#include "interactive/text_input/KillRing.hpp"
#include "terminal_events/InputEvent.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::interactive::text_input {

struct EditorOptions {
  bool multiline=false;
  bool disableCursorMovementForUpDownKeys=false;
  bool disableEscapeDoublePress=false;
  bool fullscreen=false;
  bool appleTerminal=false;
  double columns=80;
  std::string cursorChar=" ",mask;
  std::optional<std::size_t> maxVisibleLines;
};
struct NotificationRequest {
  std::string key,text,priority;
  int timeoutMs=0;
};
struct InlineGhostText {
  std::string text;
  std::size_t insertPosition=0;
};
struct EditorCallbacks {
  std::function<void(const std::string&)> onChange;
  std::function<void(std::size_t)> onOffsetChange;
  std::function<void(const std::string&)> onSubmit;
  std::function<void()> onExit;
  std::function<void(bool,const std::string&)> onExitMessage;
  std::function<void()> onHistoryUp,onHistoryDown,onHistoryReset,onClearInput;
  std::function<void(const std::string&)> addToHistory;
  std::function<void(const NotificationRequest&)> addNotification;
  std::function<void(const std::string&)> removeNotification;
  std::function<void()> markBackslashReturnUsed,prewarmModifiers;
  std::function<bool()> isShiftPressed;
  std::function<std::string(const std::string&)> stripAnsi;
  std::function<std::string(const std::string&,const terminal_events::Key&)> inputFilter;
  std::function<std::string(const std::string&)> invert;
  std::function<double()> nowMs;
};
struct EditorView {
  std::string renderedValue;
  std::size_t offset=0,viewportCharOffset=0,viewportCharEnd=0;
  std::ptrdiff_t cursorLine=0;
  double cursorColumn=0;
};

class TextEditor {
public:
  TextEditor(event_loop::Loop& owner,KillRing& killRing,
             MetricsServices metrics,EditorOptions options,
             EditorCallbacks callbacks);
  ~TextEditor();
  void setOptions(EditorOptions options);
  void onParsedKey(const terminal_events::ParsedKey& key,
                   interactive::InputState& state);
  void onInput(const std::string& input,const terminal_events::Key& key,
               interactive::InputState& state);
  EditorView view(const interactive::InputState& state,
                  const std::optional<InlineGhostText>& ghost={},
                  const std::function<std::string(const std::string&)>& dim={})const;
private:
  struct DoublePress {
    event_loop::Loop::TimerHandle timeout;
    double lastPress=0;
  };
  void press(DoublePress& tracker,
      const std::function<void(bool)>& pending,
      const std::function<void()>& doubled,
      const std::function<void()>& first={});
  std::optional<Cursor> mapKey(const std::string& input,
      const terminal_events::Key& key,const Cursor& cursor,
      interactive::InputState& state);
  std::optional<Cursor> handleCtrl(const std::string& input,
      const Cursor& cursor,interactive::InputState& state);
  std::optional<Cursor> handleMeta(const std::string& input,
      const Cursor& cursor);
  std::optional<Cursor> handleEnter(const terminal_events::Key& key,
      const Cursor& cursor,const interactive::InputState& state);
  Cursor upOrHistoryUp(const Cursor& cursor);
  Cursor downOrHistoryDown(const Cursor& cursor);
  Cursor killToLineEnd(const Cursor& cursor);
  Cursor killToLineStart(const Cursor& cursor);
  Cursor killWordBefore(const Cursor& cursor);
  Cursor yank(const Cursor& cursor);
  Cursor yankPop(const Cursor& cursor);
  void change(interactive::InputState& state,const std::string& text);
  void setOffset(interactive::InputState& state,std::size_t offset);
  event_loop::Loop& owner_;
  KillRing& killRing_;
  MetricsServices metrics_;
  EditorOptions options_;
  EditorCallbacks callbacks_;
  DoublePress ctrlC_,escape_,ctrlD_;
};

} // namespace batbox::interactive::text_input
