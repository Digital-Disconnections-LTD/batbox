#include "interactive/repl_runtime/Runtime.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::interactive::repl_runtime {
void Runtime::handleEnterTranscript(){
  frozen_=FrozenTranscript{services_.messages.messages()->size(),
    services_.presentation.streamingGadgetUses.size()};
}
void Runtime::handleExitTranscript(){frozen_.reset();}
void Runtime::setTranscriptScreen(bool value,bool virtualScrollActive){
  transcriptScreen_=value;
  virtualScrollActive_=virtualScrollActive;
  observeTranscriptMode();
}
void Runtime::observeTranscriptMode(){
  const bool active=transcriptScreen_&&virtualScrollActive_;
  // The effect runs once on mount and on each active-mode transition.
  if(transcriptModeObserved_&&inTranscriptObserved_==active)return;
  transcriptModeObserved_=true;
  inTranscriptObserved_=active;
  if(!active){
    searchQuery_.clear();
    searchCount_=0;
    searchCurrent_=0;
    searchOpen_=false;
    ++editorGeneration_;
    if(transcript_.cancelEditorTimer)transcript_.cancelEditorTimer();
    dumpMode_=false;
    editorStatus_.clear();
  }
  if(transcript_.setHighlight)
    transcript_.setHighlight(active?searchQuery_:std::string{});
  if(!active&&transcript_.clearPositions)
    transcript_.clearPositions();
  if(transcript_.requestRedraw)transcript_.requestRedraw();
}
void Runtime::setTranscriptSearch(std::string query,bool open,
    std::size_t count,std::size_t current){
  searchQuery_=std::move(query);
  searchOpen_=open;
  searchCount_=count;
  searchCurrent_=current;
  const bool active=transcriptScreen_&&virtualScrollActive_;
  if(transcript_.setHighlight)
    transcript_.setHighlight(active?searchQuery_:std::string{});
  if(!active&&transcript_.clearPositions)
    transcript_.clearPositions();
  if(transcript_.requestRedraw)transcript_.requestRedraw();
}
std::vector<TurnMessageRef> Runtime::transcriptMessages()const{
  const auto& messages=*services_.messages.messages();
  const auto length=frozen_?
    std::min(frozen_->messagesLength,messages.size()):messages.size();
  return {messages.begin(),messages.begin()+length};
}
std::size_t Runtime::transcriptStreamingGadgetUsesLength()const{
  const auto length=services_.presentation.streamingGadgetUses.size();
  return frozen_?std::min(frozen_->streamingGadgetUsesLength,length):length;
}
} // namespace batbox::interactive::repl_runtime
