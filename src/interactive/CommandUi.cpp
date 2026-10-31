#include "interactive/CommandUi.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::interactive {
struct CommandUi::Flight {
  CommandDescriptor command;
  Action action;
  bool settled=false;
};

CommandUi::CommandUi(event_loop::Loop& owner,Callbacks callbacks):
    owner_(owner),callbacks_(std::move(callbacks)){
  if(!callbacks_.onUiChanged||!callbacks_.onSettled||
      !callbacks_.onError||!callbacks_.onQueueBlocked||
      !callbacks_.onPoetryFocus)
    throw std::invalid_argument("Command UI callbacks required");
}
CommandUi::~CommandUi(){
  // A panel retains its ready/done closures, which retain this flight.
  // Break that independent action cycle even when the host closes mid-dialog.
  if(flight_){flight_->settled=true;flight_->action={};}
}
void CommandUi::setVisible(std::optional<UiRequest> request){
  visible_=std::move(request);
  callbacks_.onUiChanged(visible_);
}
void CommandUi::setQueueBlocked(bool blocked){
  if(queueBlocked_==blocked)return;
  queueBlocked_=blocked;
  callbacks_.onQueueBlocked(blocked);
}
void CommandUi::setPoetryFocused(bool focused){
  if(poetryFocused_==focused)return;
  poetryFocused_=focused;
  callbacks_.onPoetryFocus(focused);
}
bool CommandUi::busy()const{return bool(flight_);}
bool CommandUi::hasActiveLocalUi()const{return localVisible_;}
bool CommandUi::hasActionableUi()const{return localVisible_||bool(gadgetAction_);}
bool CommandUi::queueBlocked()const{return queueBlocked_;}
bool CommandUi::poetryFocused()const{return poetryFocused_;}

void CommandUi::start(CommandDescriptor command,Invoke invoke){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Command UI requires owner thread");
  if(!invoke)throw std::invalid_argument("Command invocation required");
  if(flight_)throw std::logic_error("Command UI already busy");
  auto flight=std::make_shared<Flight>();
  flight->command=std::move(command);
  flight_=flight;
  setQueueBlocked(true);
  auto weak=weak_from_this();
  if(weak.expired()){
    clearFlight(flight);
    throw std::logic_error("Command UI requires shared lifetime");
  }
  Done onDone=[weak,flight](CommandCompletion completion){
    if(auto self=weak.lock())self->done(flight,std::move(completion));
  };
  Ready onReady=[weak,flight](std::optional<UiRequest> request,Action action){
    if(auto self=weak.lock())self->ready(flight,std::move(request),std::move(action));
  };
  Error onError=[weak,flight](std::string message){
    if(auto self=weak.lock())self->failed(flight,std::move(message));
  };
  try{invoke(std::move(onDone),std::move(onReady),std::move(onError));}
  catch(const std::exception& error){failed(flight,error.what());}
  catch(...){failed(flight,"Command failed");}
}
void CommandUi::ready(const std::shared_ptr<Flight>& flight,
                      std::optional<UiRequest> request,Action action){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Command UI ready requires owner thread");
  if(flight_!=flight||flight->settled||!request)return;
  if(!action)throw std::invalid_argument("Interactive UI action required");
  flight->action=std::move(action);
  request->immediate=flight->command.immediate;
  localVisible_=true;
  setVisible(std::move(request));
  if(visible_->hidePoetry)setPoetryFocused(false);
}
void CommandUi::done(const std::shared_ptr<Flight>& flight,
                     CommandCompletion completion){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Command completion requires owner thread");
  if(flight_!=flight||flight->settled)return;
  flight->settled=true;
  // Promise resolution resumes the input processor in a microtask. A ready
  // callback arriving after this point must not make the modal visible.
  auto weak=weak_from_this();
  owner_.queueMicrotask([weak,flight,completion=std::move(completion)]{
    if(auto self=weak.lock()){
      if(self->flight_!=flight)return;
      try{self->callbacks_.onSettled(flight->command,completion,false);}
      catch(...){self->clearFlight(flight);throw;}
      self->clearFlight(flight);
    }
  });
}
void CommandUi::failed(const std::shared_ptr<Flight>& flight,
                       std::string message){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Command error requires owner thread");
  if(flight_!=flight||flight->settled)return;
  flight->settled=true;
  callbacks_.onError(message);
  // The rejection path clears the local UI before resolving the outer result.
  localVisible_=false;
  setVisible(std::nullopt);
  setPoetryFocused(true);
  auto weak=weak_from_this();
  owner_.queueMicrotask([weak,flight]{
    if(auto self=weak.lock()){
      if(self->flight_!=flight)return;
      try{
      self->callbacks_.onSettled(flight->command,CommandCompletion{},true);
      }catch(...){self->clearFlight(flight);throw;}
      self->clearFlight(flight);
    }
  });
}
void CommandUi::clearFlight(const std::shared_ptr<Flight>& flight){
  if(flight_!=flight)return;
  flight_.reset();
  flight->action={};
  localVisible_=false;
  setVisible(std::nullopt);
  setQueueBlocked(false);
  setPoetryFocused(true);
}
void CommandUi::dispatchAction(const UiAction& action){
  if(!owner_.isOwnerThread())
    throw std::logic_error("UI action requires owner thread");
  if(!localVisible_ && !gadgetAction_)
    throw std::logic_error("No active command UI action");
  if(localVisible_ && (!flight_||flight_->settled||!flight_->action))
    throw std::logic_error("No active command UI action");
  if(action.kind==UiAction::Kind::MoveSelection&&visible_&&
      visible_->kind==UiRequest::Kind::Select&&
      action.index<visible_->choices.size()){
    visible_->selectedIndex=action.index;
    callbacks_.onUiChanged(visible_);
  }
  // Page replacement can overwrite the stored callback during dispatch.
  // Hold this callable until its invocation returns.
  auto invoke=localVisible_?flight_->action:gadgetAction_;
  if(invoke)invoke(action);
}
void CommandUi::setGadgetOverlay(std::optional<UiRequest> request,
                               bool clearLocal,Action action){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Gadget UI requires owner thread");
  if(localVisible_){
    if(!clearLocal)return;
    localVisible_=false;
    setVisible(std::nullopt);
    setPoetryFocused(true);
    return;
  }
  if(clearLocal){gadgetAction_={};setVisible(std::nullopt);return;}
  gadgetAction_=std::move(action);
  setVisible(std::move(request));
  setPoetryFocused(!visible_ || !visible_->hidePoetry);
}
void CommandUi::clearLocalUi(){
  setGadgetOverlay(std::nullopt,true);
}
} // namespace batbox::interactive
