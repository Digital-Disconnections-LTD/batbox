#pragma once

#include "interactive/CommandUi.hpp"
#include "interactive/PresentationState.hpp"
#include "keyboard_input/Keyboard.hpp"
#include "terminal_ftxui/Driver.hpp"
#include "terminal_ftxui/PresentationView.hpp"
#include "terminal_ftxui/ScrollbackProjection.hpp"
#include <functional>
#include <memory>
#include <string_view>

namespace batbox::interactive {

// App-owned imports. Poetry editing has a separate indigenous owner because its
// cursor/history/kill-ring rules are broader than terminal byte dispatch.
struct InteractionImports {
  PresentationImports presentation;
  std::function<void(keyboard_input::KeyRef,InputState&)> editPoetryKey;
  std::function<void(termio::ResponseRef)> onTerminalResponse;
  std::function<void(keyboard_input::MouseRef)> onMouse;
  std::function<void(bool)> onTerminalFocus;
  std::function<void()> onSuspend;
  std::function<void(bool)> onQueueBlocked;
  std::function<void(const CommandDescriptor&,const CommandCompletion&,bool)> onCommandSettled;
  std::function<void(const std::string&)> onCommandError;
  std::function<void(std::string_view)> writePlain;
  std::function<void()> onExit;
};

class InteractionController final:
    public std::enable_shared_from_this<InteractionController> {
public:
  static std::shared_ptr<InteractionController> create(
      event_loop::Loop& owner,terminal_ftxui::Options options,
      PresentationState& state,InteractionImports imports);
  void start();
  void stop();
  void requestRedraw();
  void applyStream(StreamInput input);
  void onRawInput(std::string_view bytes);
  void onParsedInput(const keyboard_input::ParsedInput& input);
  void setInput(std::string text,std::size_t caretUtf16);
  void scroll(std::ptrdiff_t deltaRows);
  terminal_ftxui::Driver& driver(){return *driver_;}
  CommandUi& commands(){return *commands_;}
  const PresentationState& state()const{return state_;}
private:
  InteractionController(event_loop::Loop& owner,
      terminal_ftxui::Options options,PresentationState& state,
      InteractionImports imports);
  void initialize();
  void onKey(const keyboard_input::KeyRef& key);
  void onModalKey(const keyboard_input::KeyRef& key);
  void onResize(ftxui::Dimensions size);
  void commitInlineScrollback();
  void syncFocus();
  event_loop::Loop& owner_;
  terminal_ftxui::Options options_;
  PresentationState& state_;
  InteractionImports imports_;
  keyboard_input::KeyParseState parser_;
  terminal_ftxui::PlainCursor plainCursor_;
  terminal_ftxui::ScrollbackCursor scrollbackCursor_;
  std::shared_ptr<terminal_ftxui::Driver> driver_;
  std::shared_ptr<CommandUi> commands_;
  bool terminalFocused_=true,poetryDesiredFocus_=true;
};

} // namespace batbox::interactive
