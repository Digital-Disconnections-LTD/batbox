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
  std::function<void()> resetMicrocompactState,resetContextCollapse,onCancel;
  std::function<bool(const std::string&)> feature;
  std::function<void(const Json&)> setPastedContents;
  std::function<bool(const TurnMessageRef&)> selectableUserMessage;
  std::function<event_loop::Deferred<bool>(value::ValueRef,
    const std::string&)> fileHistoryHasAnyChanges;
  std::function<bool(const TurnMessagesRef&,std::size_t)>
    messagesAfterAreOnlySynthetic;
  std::function<void(TurnMessageRef)> publishPreselect;
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
  void setMessageSelectorPreselect(TurnMessageRef value);
  event_loop::Deferred<event_loop::Unit> editMessage(
    const std::string& renderUuid,const TurnMessagesRef& displayed,
    value::ValueRef fileHistory);
  bool selectorVisible()const{return selectorVisible_;}
  const TurnMessageRef& preselect()const{return preselect_;}
private:
  RewindServices services_;
  bool selectorVisible_=false;
  TurnMessageRef preselect_;
  void setSelectorVisible(bool value);
  void setPreselect(TurnMessageRef value);
};
} // namespace batbox::interactive::navigation
