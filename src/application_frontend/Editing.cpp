#include "application_frontend/Defaults.hpp"
#include "messages/TextContent.hpp"
#include "interactive/conversation_shell/Shell.hpp"
#include "application_frontend/RewindModal.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::application_frontend {
namespace {
template<class T>event_loop::Deferred<T> failure(event_loop::Loop& owner,
    const std::string& message){
  event_loop::Deferred<T> result(owner);
  result.reject(std::make_exception_ptr(std::runtime_error(message)));
  return result;
}
std::string preview(const interactive::TurnMessageRef& message){
  const auto text=messages::getContentText(
    value::property(value::property(*message,"message"),"content")).value_or("");
  std::string line;
  for(const char c:text){
    if(c=='\r'||c=='\n'){if(!line.empty())break;continue;}
    line+=c;if(line.size()>=72)break;
  }
  if(line.empty())line="(image or empty message)";
  if(text.size()>line.size())line+="…";
  return line;
}
}
void Bundle::initializeRewind(){
  if(rewind_)return;
  if(!inputs_.getRuntime||!inputs_.getShell)
    throw std::runtime_error("Transcript editing requires runtime and shell getters");
  auto runtime=inputs_.getRuntime();
  if(!runtime)throw std::runtime_error("Transcript runtime is unavailable");
  interactive::navigation::RewindServices services(owner_,runtime->messages(),store_);
  services.randomUUID=[messages=inputs_.messages]{return messages->randomUUID();};
  services.setConversationId=[weak=weak_from_this()](const std::string& value){
    if(auto self=weak.lock()){
      self->state_.conversationId=value;++self->state_.revision;
      if(auto shell=self->inputs_.getShell())shell->messagesChanged();
      self->inputs_.redraw();
    }
  };
  services.setInputValue=[weak=weak_from_this()](const std::string& value){
    if(auto self=weak.lock()){
      self->state_.input.text=value;
      self->state_.input.caretUtf16=value::stringLength(value);
      ++self->state_.revision;self->inputs_.redraw();
    }
  };
  services.setInputMode=[weak=weak_from_this()](const std::string& value){
    if(auto self=weak.lock()){
      if(value=="bash"&& !self->state_.input.text.starts_with('!')){
        self->state_.input.text.insert(self->state_.input.text.begin(),'!');
        self->state_.input.caretUtf16=value::stringLength(self->state_.input.text);
      }
      ++self->state_.revision;self->inputs_.redraw();
    }
  };
  services.setPastedContents=[weak=weak_from_this()](const Json& value){
    if(auto self=weak.lock()){
      self->state_.input.pastedContents=value;
      ++self->state_.revision;self->inputs_.redraw();
    }
  };
  services.resetMicrocompactState=inputs_.resetMicrocompactState?
    inputs_.resetMicrocompactState:[]{};
  services.resetContextCollapse=inputs_.resetContextCollapse?
    inputs_.resetContextCollapse:[]{};
  services.feature=inputs_.feature;
  services.onCancel=[weak=weak_from_this()]{
    if(auto self=weak.lock())if(auto shell=self->inputs_.getShell()){
      shell->actions().setCursor({});
      shell->setTranscript(false);
      shell->jumpToBottom();
    }
  };
  services.selectableUserMessage=selectableUserMessage;
  services.messagesAfterAreOnlySynthetic=messagesAfterAreOnlySynthetic;
  services.fileHistoryHasAnyChanges=[weak=weak_from_this()](value::ValueRef history,
      const std::string& id){
    auto self=weak.lock();
    if(!self)throw std::runtime_error("Transcript editor has stopped");
    if(self->inputs_.fileHistory&&self->inputs_.fileHistory->enabled())
      return self->inputs_.fileHistory->hasAnyChanges(id);
    if(self->inputs_.fileHistoryHasAnyChanges)
      return self->inputs_.fileHistoryHasAnyChanges(std::move(history),id);
    if(self->inputs_.feature("FILE_HISTORY"))
      return failure<bool>(self->owner_,
        "File-history checks are enabled without a native provider");
    event_loop::Deferred<bool> result(self->owner_);result.resolve(false);return result;
  };
  services.publishPreselect=[](interactive::TurnMessageRef){};
  services.publishSelectorVisible=[weak=weak_from_this()](bool visible){
    if(auto self=weak.lock())if(visible)self->showRewindSelector();
  };
  rewind_=std::make_shared<interactive::navigation::Rewind>(std::move(services));
}
event_loop::Deferred<event_loop::Unit> Bundle::editMessage(
    interactive::MessageRef message){
  if(!owner_.isOwnerThread())throw std::logic_error("Transcript edit owner required");
  try{
    initializeRewind();
    if(!message)return failure<event_loop::Unit>(owner_,"Message to edit is missing");
    const auto& uuid=value::property(*message,"uuid");
    if(!uuid.is_string())return failure<event_loop::Unit>(owner_,
      "Message to edit has no UUID");
    return rewind_->editMessage(uuid.get<std::string>(),
      inputs_.getRuntime()->messages().messages(),{});
  }catch(const std::exception& error){
    return failure<event_loop::Unit>(owner_,error.what());
  }
}
void Bundle::openRewind(){
  if(!owner_.isOwnerThread())throw std::logic_error("Rewind owner required");
  initializeRewind();
  if(!rewind_->selectorVisible())rewind_->handleShowMessageSelector();
}
void Bundle::showRewindSelector(){
  auto runtime=inputs_.getRuntime();auto shell=inputs_.getShell();
  if(!runtime||!shell)throw std::runtime_error("Rewind view is unavailable");
  std::vector<interactive::TurnMessageRef> choices;
  const auto& messages=*runtime->messages().messages();
  for(auto it=messages.rbegin();it!=messages.rend();++it)
    if(selectableUserMessage(*it))choices.push_back(*it);
  if(choices.empty()){
    rewind_->setMessageSelectorVisible(false);
    throw std::runtime_error("No editable user messages");
  }
  interactive::UiRequest request;
  request.kind=interactive::UiRequest::Kind::Select;
  request.id="rewind-message-selector";
  request.title="Edit an earlier message";
  request.body="Choose a message. Later conversation messages will be removed.";
  request.hidePoetry=true;
  for(const auto& message:choices)request.choices.push_back(preview(message));
  if(const auto& selected=rewind_->preselect()){
    const auto found=std::find(choices.begin(),choices.end(),selected);
    if(found!=choices.end())request.selectedIndex=
      static_cast<std::size_t>(std::distance(choices.begin(),found));
  }
  // A local slash command returns "skip" after requesting this selector.
  // Its submit completion clears the transient local UI in the current turn;
  // publish the indigenous selector in the next owner turn so it remains visible.
  auto weak=weak_from_this();
  owner_.queueNextTurn([weak,request=std::move(request),
      choices=std::move(choices)]()mutable{
    auto self=weak.lock();
    if(!self||!self->rewind_||!self->rewind_->selectorVisible())return;
    auto shell=self->inputs_.getShell();
    if(!shell)return;
    try{shell->commands().start({"rewind","",false,false},
    [self,request=std::move(request),choices=std::move(choices)](
        interactive::CommandUi::Done done,
        interactive::CommandUi::Ready ready,
        interactive::CommandUi::Error error)mutable{
      RewindModalInputs modal;
      modal.list=std::move(request);
      modal.messages=std::move(choices);
      if(const auto& selected=self->rewind_->preselect()){
        const auto found=std::find(modal.messages.begin(),
          modal.messages.end(),selected);
        if(found!=modal.messages.end()){
          modal.preselected=true;
          modal.preselectedIndex=static_cast<std::size_t>(
            std::distance(modal.messages.begin(),found));
        }
      }
      modal.history=self->inputs_.fileHistory;
      modal.beforeRestore=[self]{
        if(auto shell=self->inputs_.getShell()){
          shell->actions().setCursor({});shell->setTranscript(false);
          shell->jumpToBottom();
        }
      };
      modal.restoreConversation=[self](interactive::TurnMessageRef message){
        self->rewind_->handleRestoreMessage(message);};
      modal.close=[self]{self->rewind_->setMessageSelectorVisible(false);};
      modal.done=std::move(done);
      modal.ready=std::move(ready);
      modal.error=std::move(error);
      std::make_shared<RewindModal>(std::move(modal))->start();
    });}
    catch(...){
      self->rewind_->setMessageSelectorVisible(false);
      if(self->inputs_.onError)self->inputs_.onError(std::current_exception());
    }
  });
}
} // namespace batbox::application_frontend
