#pragma once
#include "interactive/PresentationState.hpp"
#include "interactive/QueryTurn.hpp"

namespace batbox::interactive::lifecycle {
struct MessageServices {
  std::function<void(const TurnMessagesRef&)> publishMessages;
  std::function<void(const std::optional<std::string>&)>
    publishProcessingInput;
};

class MessageState {
public:
  explicit MessageState(TurnMessagesRef initial,MessageServices services={});
  const TurnMessagesRef& messages()const{return messages_;}
  std::size_t baseline()const{return userInputBaseline_;}
  bool userMessagePending()const{return userMessagePending_;}
  const std::optional<std::string>& processingInput()const{return processingInput_;}
  void setMessages(const TurnMessageUpdate& action);
  void setMessages(TurnMessagesRef next);
  void setUserInputOnProcessing(std::optional<std::string> input);
  std::optional<std::string> placeholderText(
    std::size_t displayedMessagesLength,bool viewingBatMission)const;
  static bool isHumanTurn(const TurnMessageRef& message);
private:
  TurnMessagesRef messages_;
  MessageServices services_;
  std::size_t userInputBaseline_=0;
  bool userMessagePending_=false;
  std::optional<std::string> processingInput_;
};

MessageServices bindPresentationMessages(PresentationState& presentation,
  std::function<void()> requestRedraw,
  std::function<void(const std::optional<std::string>&)> publishProcessingInput={});
} // namespace batbox::interactive::lifecycle
