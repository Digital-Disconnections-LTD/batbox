#include "application_runtime/ApplicationRuntime.hpp"
#include "interactive/conversation_shell/Shell.hpp"
#include <stdexcept>

namespace batbox::application_runtime {
void ApplicationRuntime::bindShellImports(
    interactive::conversation_shell::Imports& imports){
  if(&imports.owner!=&inputs_.owner||
      &imports.presentation!=&inputs_.presentation)
    throw std::invalid_argument("Shell and runtime must share one owner and presentation");
  imports.submitServices=submit_;
  auto weak=weak_from_this();
  imports.interruptTurn=[weak]{
    auto self=weak.lock();
    if(!self||!self->activeAbort_)return false;
    // A repeat Esc while the turn unwinds is still taken; the row keeps
    // showing the interrupt instead of falling through to the input.
    if(self->activeAbort_->aborted())return true;
    self->interruptedTurn_=self->activeAbort_;
    self->activeAbort_->abort("interrupt");
    return true;
  };
  imports.turnInterrupting=[weak]{
    auto self=weak.lock();
    return self&&self->activeAbort_&&self->activeAbort_->aborted();
  };
  imports.takeQueued=[weak](bool newestOnly,const std::string& input,std::size_t caret)
      ->std::optional<interactive::submit::PopEditableResult>{
    auto self=weak.lock();
    if(!self)return {};
    return self->takeQueued(newestOnly,input,caret);
  };
  imports.makeSubmitRequest=[weak](const std::string& text,
      const interactive::InputState& input){
    auto self=weak.lock();
    if(!self)throw std::logic_error("Application runtime no longer active");
    return self->poetryRequest(text,input);
  };
}
void ApplicationRuntime::attachShell(
    std::shared_ptr<interactive::conversation_shell::Shell> shell){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("Shell attachment requires owner thread");
  if(!shell)throw std::invalid_argument("Conversation shell required");
  std::weak_ptr<interactive::conversation_shell::Shell> weak=shell;
  setStreamSink([weak](interactive::StreamInput input){
    if(auto active=weak.lock())active->applyStream(std::move(input));
  });
  setRedrawSink([weak]{if(auto active=weak.lock())active->requestRedraw();});
  shell->bindRuntime(runtime_);
}
} // namespace batbox::application_runtime
