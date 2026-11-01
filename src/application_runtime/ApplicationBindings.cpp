#include "application_runtime/ApplicationRuntime.hpp"
#include "application/Async.hpp"
#include <stdexcept>

namespace batbox::application_runtime {
std::shared_ptr<interactive::context::ContextFactory>
ApplicationRuntime::makeContextFactory(
    interactive::context::ContextBindings bindings){
  bindings.store=inputs_.store;
  bindings.readFileState=inputs_.readFileState;
  bindings.feature=inputs_.feature;
  auto weak=weak_from_this();
  bindings.setMessages=[weak](const interactive::TurnMessageUpdate& update){
    if(auto self=weak.lock())self->messages_->setMessages(update);
  };
  bindings.setResponseLength=[weak,owner=inputs_.owner.dispatcher()](
      const std::function<double(double)>& update){
    application::onOwnerAndWait<void>(owner,[weak,update]{
      if(auto self=weak.lock())self->loading_->setResponseLength(update);
    });
  };
  bindings.setLocalUi=[weak](std::optional<interactive::UiRequest> value){
    if(auto self=weak.lock()){
      if(self->modalSink_)self->modalSink_(value);
      self->inputs_.presentation.modal=std::move(value);
      ++self->inputs_.presentation.revision;self->requestRedraw();
    }
  };
  bindings.setStreamMode=[weak](const std::string& value){
    if(auto self=weak.lock()){
      self->inputs_.presentation.streamMode=value;
      ++self->inputs_.presentation.revision;self->requestRedraw();
    }
  };
  bindings.setConversationId=[weak](const std::string& value){
    if(auto self=weak.lock()){
      self->inputs_.presentation.conversationId=value;
      ++self->inputs_.presentation.revision;self->requestRedraw();
    }
  };
  return std::make_shared<interactive::context::ContextFactory>(
    std::move(bindings));
}
void ApplicationRuntime::bindContext(){
  context_=makeContextFactory(std::move(inputs_.context));
  // A moved std::function may still retain its captures. Release the input
  // copies so they cannot pin a retired Roost after a context replacement.
  inputs_.context={};
}
void ApplicationRuntime::bindQuery(){
  auto weak=weak_from_this();
  turn_=std::make_shared<interactive::QueryTurnServices>(std::move(inputs_.turn));
  execution_=std::make_shared<interactive::QueryExecutionServices>(
    std::move(inputs_.execution));
  turn_->feature=inputs_.feature;
  execution_->feature=inputs_.feature;
  if(inputs_.createInterruptionMessage){
    const auto completed=turn_->mrOnTurnComplete;
    if(!completed)throw std::invalid_argument("Turn-complete callback required");
    turn_->mrOnTurnComplete=[weak,completed](
        const interactive::TurnMessagesRef& messages,bool aborted){
      auto self=weak.lock();
      if(self&&aborted&&self->activeAbort_&&
          self->interruptedTurn_.lock()==self->activeAbort_){
        self->interruptedTurn_.reset();
        const auto notice=self->inputs_.createInterruptionMessage();
        self->messages_->setMessages([notice](const auto& previous){
          auto next=std::make_shared<interactive::TurnMessages>(*previous);
          next->push_back(notice);return next;
        });
        return completed(self->messages_->messages(),aborted);
      }
      return completed(messages,aborted);
    };
  }
  turn_->enqueue=[weak](const Json& command){
    if(auto self=weak.lock())self->queue_->enqueue(command);
  };
  turn_->resetTimingRefs=[weak]{if(auto self=weak.lock()){
    self->loading_->resetTimingRefs();
    self->turnState_->loadingStartTime=self->loading_->loadingStartTime();
    self->turnState_->totalPausedMs=0;
  }};
  turn_->setMessages=[weak](const interactive::TurnMessageUpdate& update){
    if(auto self=weak.lock())self->messages_->setMessages(update);
  };
  turn_->getMessages=[weak]{
    if(auto self=weak.lock())return self->messages_->messages();
    return interactive::TurnMessagesRef{};
  };
  turn_->setResponseLengthRef=[weak](double value){
    if(auto self=weak.lock())self->loading_->setResponseLength(
      [value](double){return value;});
  };
  turn_->clearApiMetrics=[weak]{if(auto self=weak.lock()){
    self->inputs_.presentation.apiMetrics.clear();
    ++self->inputs_.presentation.revision;self->requestRedraw();
  }};
  turn_->clearStreamingGadgetUses=[weak]{if(auto self=weak.lock()){
    self->inputs_.presentation.streamingGadgetUses.clear();
    ++self->inputs_.presentation.revision;self->requestRedraw();
  }};
  turn_->clearStreamingText=[weak]{if(auto self=weak.lock()){
    self->inputs_.presentation.streamingText.reset();
    ++self->inputs_.presentation.revision;self->requestRedraw();
  }};
  turn_->resetLoadingState=[weak]{if(auto self=weak.lock()){
    self->turnState_->totalPausedMs=self->loading_->totalPausedMs();
    self->loading_->resetLoadingState();
  }};
  turn_->clearAbortController=[weak]{if(auto self=weak.lock())
    self->activeAbort_.reset();};
  turn_->getInputValue=[weak]{
    if(auto self=weak.lock())return self->inputs_.presentation.input.text;
    return std::string{};
  };
  turn_->getCommandQueueLength=[weak]{
    if(auto self=weak.lock())return self->queue_->size();
    return std::size_t{};
  };
  turn_->now=inputs_.nowMs;

  execution_->store=inputs_.store;
  execution_->getMessages=turn_->getMessages;
  execution_->resetLoadingState=turn_->resetLoadingState;
  execution_->clearAbortController=turn_->clearAbortController;
  auto previousEvent=execution_->onQueryEvent;
  execution_->onQueryEvent=[weak,previousEvent](const query::AsyncEngineStep& step){
    if(auto self=weak.lock())self->acceptStep(step);
    if(previousEvent)previousEvent(step);
  };
  auto previousConversation=execution_->setConversationId;
  execution_->setConversationId=[weak,previousConversation](const std::string& value){
    if(auto self=weak.lock()){
      self->inputs_.presentation.conversationId=value;
      ++self->inputs_.presentation.revision;self->requestRedraw();
    }
    if(previousConversation)previousConversation(value);
  };
  auto previousBlocked=execution_->setContextBlocked;
  execution_->setContextBlocked=[weak,previousBlocked](bool value){
    if(auto self=weak.lock()){
      self->inputs_.presentation.contextBlocked=value;
      ++self->inputs_.presentation.revision;self->requestRedraw();
    }
    if(previousBlocked)previousBlocked(value);
  };
  interactive::bindQueryContextFactory(*execution_,context_);
  if(inputs_.onQueryContextCreated){
    const auto make=execution_->getGadgetUseContext;
    execution_->getGadgetUseContext=[make,publish=inputs_.onQueryContextCreated](
        const interactive::TurnMessagesRef& messages,
        const interactive::TurnRequest& request){
      auto context=make(messages,request);publish(context);return context;
    };
  }
  interactive::bindQueryExecutionEngine(inputs_.owner,*execution_,
    inputs_.engine,inputs_.notifyLifecycle);
  interactive::bindQueryTurnExecution(inputs_.owner,*turn_,executionState_,
    inputs_.makeQueryParams,execution_);
}
void ApplicationRuntime::installCommands(
    std::vector<user_input::CommandEntry> commands,
    std::function<void()> onInstalled){
  if(turnState_->guard.isActive()){
    pendingCommands_=std::move(commands);
    pendingCommandsInstalled_=std::move(onInstalled);
    if(unsubscribeGuard_)return;
    auto weak=weak_from_this();
    unsubscribeGuard_=turnState_->guard.subscribe(
      std::make_shared<bootstrap::Signal<>::Listener>([weak]{
        auto self=weak.lock();
        if(!self||self->turnState_->guard.isActive()||
           !self->pendingCommands_)return;
        try{
          auto old=std::make_shared<std::vector<user_input::CommandEntry>>();
          old->swap(self->inputs_.commands);
          self->inputs_.commands.swap(*self->pendingCommands_);
          self->pendingCommands_.reset();
          auto installed=std::move(self->pendingCommandsInstalled_);
          self->pendingCommandsInstalled_={};
          auto unsubscribe=std::move(self->unsubscribeGuard_);
          self->unsubscribeGuard_={};
          if(unsubscribe)unsubscribe();
          self->inputs_.owner.queueMicrotask([old]{});
          if(installed)installed();
        }catch(...){self->inputs_.onDetachedError(std::current_exception());}
      }));
    return;
  }
  auto old=std::make_shared<std::vector<user_input::CommandEntry>>();
  old->swap(inputs_.commands);
  inputs_.commands.swap(commands);
  inputs_.owner.queueMicrotask([old]{});
  if(onInstalled)onInstalled();
}
void ApplicationRuntime::replaceCommands(
    std::vector<user_input::CommandEntry> commands,
    std::function<void()> onInstalled){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("Command replacement requires owner loop");
  if(stopping_)
    throw std::logic_error("Cannot replace commands during shutdown");
  installCommands(std::move(commands),std::move(onInstalled));
}
void ApplicationRuntime::rebindEnvironment(EnvironmentRebind next){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("Environment rebind requires owner loop");
  if(stopping_)
    throw std::logic_error("Cannot replace an environment during shutdown");
  if(!canReplaceConversation())
    throw std::logic_error("Cannot replace environment during a model turn");
  if(!next.engine||next.config.model.empty())
    throw std::invalid_argument("Rebind requires query engine and model");
  auto replacementContext=makeContextFactory(std::move(next.context));
  auto replacementExecution=
    std::make_shared<interactive::QueryExecutionServices>(*execution_);
  interactive::bindQueryContextFactory(*replacementExecution,
    replacementContext);
  if(inputs_.onQueryContextCreated){
    const auto make=replacementExecution->getGadgetUseContext;
    replacementExecution->getGadgetUseContext=
      [make,publish=inputs_.onQueryContextCreated](
        const interactive::TurnMessagesRef& messages,
        const interactive::TurnRequest& request){
        auto context=make(messages,request);publish(context);return context;
      };
  }
  interactive::bindQueryExecutionEngine(inputs_.owner,*replacementExecution,
    next.engine,next.notifyLifecycle);
  auto replacementState=std::make_shared<interactive::QueryExecutionState>();
  inputs_.config=std::move(next.config);
  inputs_.engine=std::move(next.engine);
  inputs_.notifyLifecycle=std::move(next.notifyLifecycle);
  context_=std::move(replacementContext);
  execution_=std::move(replacementExecution);
  executionState_=std::move(replacementState);
  interactive::bindQueryTurnExecution(inputs_.owner,*turn_,executionState_,
    inputs_.makeQueryParams,execution_);
  installCommands(std::move(next.commands),
    std::move(next.onCommandsInstalled));
}
} // namespace batbox::application_runtime
