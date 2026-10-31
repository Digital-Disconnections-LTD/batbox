#include "interactive/lifecycle/MessageState.hpp"
#include "interactive/TranscriptRows.hpp"
#include <stdexcept>

namespace batbox::interactive::lifecycle {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
}
MessageState::MessageState(TurnMessagesRef initial,MessageServices services):
  messages_(std::move(initial)),services_(std::move(services)){
  if(!messages_)throw std::invalid_argument("Initial message list required");
}
bool MessageState::isHumanTurn(const TurnMessageRef& message){
  if(!message)return false;
  return field(*message,"type")=="user"&&
    !value::truthy(field(*message,"isMeta"))&&
    (!message->is_object()||!message->contains("toolUseResult")||
      message->at("toolUseResult").is_discarded());
}
void MessageState::setMessages(const TurnMessageUpdate& action){
  if(!action)throw std::invalid_argument("Message updater required");
  setMessages(action(messages_));
}
void MessageState::setMessages(TurnMessagesRef next){
  if(!next)throw std::invalid_argument("Message list required");
  const auto previous=messages_;
  messages_=std::move(next);
  if(messages_->size()<userInputBaseline_){
    userInputBaseline_=0;
  }else if(messages_->size()>previous->size()&&userMessagePending_){
    const auto delta=messages_->size()-previous->size();
    const bool sharesPrefix=previous->empty()||
      (!messages_->empty()&&(*messages_)[0]==(*previous)[0]);
    const auto begin=sharesPrefix?messages_->size()-delta:0;
    bool foundHuman=false;
    for(std::size_t index=begin;index<begin+delta;++index)
      if(isHumanTurn((*messages_)[index])){foundHuman=true;break;}
    if(foundHuman)userMessagePending_=false;
    else userInputBaseline_=messages_->size();
  }
  if(services_.publishMessages)services_.publishMessages(messages_);
}
void MessageState::setUserInputOnProcessing(std::optional<std::string> input){
  if(input){userInputBaseline_=messages_->size();userMessagePending_=true;}
  else userMessagePending_=false;
  processingInput_=std::move(input);
  if(services_.publishProcessingInput)
    services_.publishProcessingInput(processingInput_);
}
std::optional<std::string> MessageState::placeholderText(
    std::size_t displayedMessagesLength,bool viewingBatMission)const{
  if(processingInput_&&!processingInput_->empty()&&!viewingBatMission&&
      displayedMessagesLength<=userInputBaseline_)
    return processingInput_;
  return {};
}
MessageServices bindPresentationMessages(PresentationState& presentation,
    std::function<void()> requestRedraw,
    std::function<void(const std::optional<std::string>&)> publishProcessingInput){
  MessageServices result;
  result.publishMessages=[&presentation,redraw=std::move(requestRedraw)](
      const TurnMessagesRef& messages){
    presentation.messages.assign(messages->begin(),messages->end());
    rebuildTranscriptRows(presentation);
    ++presentation.revision;
    if(redraw)redraw();
  };
  result.publishProcessingInput=std::move(publishProcessingInput);
  return result;
}
} // namespace batbox::interactive::lifecycle
