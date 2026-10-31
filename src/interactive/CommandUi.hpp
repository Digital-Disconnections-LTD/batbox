#pragma once

#include "event_loop/Loop.hpp"
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::interactive {

// A command supplies data for one indigenous view. The presentation reducer owns
// its layout; no rendering tree or framework object enters the command path.
struct UiRequest {
  enum class Kind {Notice, Select, Form, Confirm, Progress};
  Kind kind=Kind::Notice;
  std::string id;
  std::string title;
  std::string body;
  std::vector<std::string> choices;
  std::size_t selectedIndex=0;
  bool hidePoetry=true;
  // A form may accept credentials. Renderers receive only a masked body; this
  // marker lets terminal chrome show the private-input state explicitly.
  bool sensitiveInput=false;
  bool immediate=false;
  // Reserve action rows before showing this many one-line body previews.
  std::optional<std::size_t> bodyPreviewLines;
};

struct UiAction {
  enum class Kind {Activate, Cancel, MoveSelection, TextChange, Back};
  Kind kind=Kind::Activate;
  std::size_t index=0;
  std::string text;
};

struct CommandDescriptor {
  std::string name;
  std::string args;
  bool immediate=false;
  bool fullscreen=false;
};

struct DoneOptions {
  enum class Display {Default, System, Skip};
  Display display=Display::Default;
  bool shouldQuery=false;
  std::vector<std::string> metaMessages;
  std::optional<std::string> nextInput;
  std::optional<bool> submitNextInput;
};

struct CommandCompletion {
  std::optional<std::string> result;
  DoneOptions options;
};

class CommandUi:public std::enable_shared_from_this<CommandUi> {
public:
  using Done=std::function<void(CommandCompletion)>;
  using Action=std::function<void(const UiAction&)>;
  using Ready=std::function<void(std::optional<UiRequest>,Action)>;
  using Error=std::function<void(std::string)>;
  using Invoke=std::function<void(Done,Ready,Error)>;
  struct Callbacks {
    std::function<void(std::optional<UiRequest>)> onUiChanged;
    std::function<void(const CommandDescriptor&,const CommandCompletion&,bool failed)> onSettled;
    std::function<void(const std::string&)> onError;
    std::function<void(bool blocked)> onQueueBlocked;
    std::function<void(bool focused)> onPoetryFocus;
  };

  CommandUi(event_loop::Loop& owner,Callbacks callbacks);
  ~CommandUi();
  void start(CommandDescriptor command,Invoke invoke);
  void dispatchAction(const UiAction& action);
  // Gadget overlays cannot replace an active local command. Explicit clear is
  // reserved for the command completion path.
  void setGadgetOverlay(std::optional<UiRequest> request,bool clearLocal=false,
    Action action={});
  void clearLocalUi();
  bool busy()const;
  bool hasActiveLocalUi()const;
  bool hasActionableUi()const;
  bool queueBlocked()const;
  bool poetryFocused()const;
  const std::optional<UiRequest>& visibleUi()const{return visible_;}
private:
  struct Flight;
  void ready(const std::shared_ptr<Flight>& flight,
             std::optional<UiRequest> request,Action action);
  void done(const std::shared_ptr<Flight>& flight,
            CommandCompletion completion);
  void failed(const std::shared_ptr<Flight>& flight,std::string message);
  void clearFlight(const std::shared_ptr<Flight>& flight);
  void setVisible(std::optional<UiRequest> request);
  void setQueueBlocked(bool blocked);
  void setPoetryFocused(bool focused);
  event_loop::Loop& owner_;
  Callbacks callbacks_;
  std::shared_ptr<Flight> flight_;
  std::optional<UiRequest> visible_;
  Action gadgetAction_;
  bool localVisible_=false,queueBlocked_=false,poetryFocused_=true;
};

} // namespace batbox::interactive
