#pragma once
#include "app_state/State.hpp"
#include "interactive/lifecycle/MessageState.hpp"
#include "interactive/navigation/ResubmitText.hpp"

namespace batbox::interactive::navigation {
struct RewindServices {
  event_loop::Loop& owner;
  lifecycle::MessageState& messages;
  std::shared_ptr<app_state::AppStateStore> store;
  RewindServices(event_loop::Loop& loop,lifecycle::MessageState& messageState,
    std::shared_ptr<app_state::AppStateStore> appStore):
    owner(loop),messages(messageState),store(std::move(appStore)){}
  std::function<std::string()> randomUUID;
  std::function<void(const std::string&)> setConversationId,setInputValue,
    setInputMode;
  std::function<void()> resetMicrocompactState;
  std::function<void(const Json&)> setPastedContents;
  std::function<void(bool)> publishSelectorVisible;
};
class Rewind:public std::enable_shared_from_this<Rewind>{
public:
  explicit Rewind(RewindServices services);
  void rewindConversationTo(const TurnMessageRef& message);
  void restoreMessageSync(const TurnMessageRef& message);
  void handleRestoreMessage(const TurnMessageRef& message);
  void handleShowMessageSelector();
  void setMessageSelectorVisible(bool value);
  bool selectorVisible()const{return selectorVisible_;}
private:
  RewindServices services_;
  bool selectorVisible_=false;
  void setSelectorVisible(bool value);
};
} // namespace batbox::interactive::navigation
