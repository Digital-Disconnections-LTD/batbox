#pragma once
#include "interactive/CommandUi.hpp"
#include "user_input/SlashDispatch.hpp"

namespace batbox::user_input {
class UiCommandRouter:public std::enable_shared_from_this<UiCommandRouter> {
public:
  struct Projection {
    std::function<void(std::optional<interactive::UiRequest>)> onUiChanged;
    std::function<void(const std::string&)> onError;
    std::function<void(bool)> onQueueBlocked,onPoetryFocus;
  };
  static std::shared_ptr<UiCommandRouter> create(event_loop::Loop& loop,
    Projection projection);
  event_loop::Deferred<UiCompletion> invoke(const Json& command,
    const std::string& args,interactive::CommandUi::Invoke call);
  std::shared_ptr<interactive::CommandUi> ui()const{return ui_;}
private:
  UiCommandRouter(event_loop::Loop& loop,Projection projection);
  void initialize();
  void settle(const interactive::CommandDescriptor&,
    const interactive::CommandCompletion&,bool failed);
  event_loop::Loop& loop_;
  Projection projection_;
  std::shared_ptr<interactive::CommandUi> ui_;
  std::optional<event_loop::Deferred<UiCompletion>> pending_;
};
CommandEntry bindUiCommand(Json descriptor,
  std::shared_ptr<UiCommandRouter> router,
  std::function<interactive::CommandUi::Invoke(const std::string& args)> loadAndInvoke);
} // namespace batbox::user_input
