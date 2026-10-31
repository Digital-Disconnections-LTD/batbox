#include "interactive/InteractionController.hpp"
#include "termio/Csi.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace batbox::interactive {
namespace {
void require(const auto& fn,const char* name){
  if(!fn)throw std::invalid_argument(std::string("Interaction import required: ")+name);
}
bool isName(const keyboard_input::KeyRef& key,const char* name){
  return key->name&&*key->name==name;
}
}

InteractionController::InteractionController(event_loop::Loop& owner,
    terminal_ftxui::Options options,PresentationState& state,
    InteractionImports imports):owner_(owner),options_(options),state_(state),
    imports_(std::move(imports)){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Interaction controller requires owner thread");
  require(imports_.editPoetryKey,"editPromptKey");
  require(imports_.onTerminalResponse,"onTerminalResponse");
  require(imports_.onMouse,"onMouse");
  require(imports_.onTerminalFocus,"onTerminalFocus");
  require(imports_.onSuspend,"onSuspend");
  require(imports_.onQueueBlocked,"onQueueBlocked");
  require(imports_.onCommandSettled,"onCommandSettled");
  require(imports_.onCommandError,"onCommandError");
  require(imports_.writePlain,"writePlain");
  require(imports_.onExit,"onExit");
}
std::shared_ptr<InteractionController> InteractionController::create(
    event_loop::Loop& owner,terminal_ftxui::Options options,
    PresentationState& state,InteractionImports imports){
  auto self=std::shared_ptr<InteractionController>(
      new InteractionController(owner,options,state,std::move(imports)));
  self->initialize();
  return self;
}
void InteractionController::initialize(){
  auto weak=weak_from_this();
  CommandUi::Callbacks ui;
  ui.onUiChanged=[weak](std::optional<UiRequest> request){
    if(auto self=weak.lock()){
      self->state_.modal=std::move(request);
      ++self->state_.revision;
      self->requestRedraw();
    }
  };
  ui.onSettled=[weak](const CommandDescriptor& command,
      const CommandCompletion& result,bool failed){
    if(auto self=weak.lock())
      self->imports_.onCommandSettled(command,result,failed);
  };
  ui.onError=[weak](const std::string& error){
    if(auto self=weak.lock())self->imports_.onCommandError(error);
  };
  ui.onQueueBlocked=[weak](bool blocked){
    if(auto self=weak.lock())self->imports_.onQueueBlocked(blocked);
  };
  ui.onPoetryFocus=[weak](bool focused){
    if(auto self=weak.lock()){
      self->poetryDesiredFocus_=focused;
      self->syncFocus();
    }
  };
  commands_=std::make_shared<CommandUi>(owner_,std::move(ui));

  terminal_ftxui::Callbacks callbacks;
  callbacks.render=[weak]{
    const auto self=weak.lock();
    if(!self)throw std::logic_error("FTXUI owner expired");
    const auto& view=self->state_.viewport;
    return terminal_ftxui::renderPresentationWithScrollback(self->state_,
      ftxui::Dimensions{view.columns,view.rows},self->scrollbackCursor_,
      self->driver_->mode()==terminal_ftxui::Mode::Inline);
  };
  callbacks.onRawInputBytes=[weak](std::string_view bytes){
    if(auto self=weak.lock())self->onRawInput(bytes);
  };
  callbacks.onResize=[weak](ftxui::Dimensions size){
    if(auto self=weak.lock())self->onResize(size);
  };
  callbacks.renderPlain=[weak]{
    const auto self=weak.lock();
    if(!self)throw std::logic_error("FTXUI owner expired");
    return terminal_ftxui::drainPresentationPlain(
      self->state_,self->plainCursor_);
  };
  callbacks.writePlain=[weak](std::string_view text){
    if(auto self=weak.lock())self->imports_.writePlain(text);
  };
  callbacks.onExit=[weak]{
    if(auto self=weak.lock())self->imports_.onExit();
  };
  driver_=std::make_shared<terminal_ftxui::Driver>(owner_,options_,
    std::move(callbacks));
}
void InteractionController::start(){driver_->start();}
void InteractionController::stop(){driver_->stop();}
void InteractionController::requestRedraw(){
  if(driver_)driver_->requestRedraw();
}
void InteractionController::onResize(ftxui::Dimensions size){
  updateViewport(state_,size.dimx,size.dimy);
  commitInlineScrollback();
  requestRedraw();
}
void InteractionController::commitInlineScrollback(){
  if(!driver_||driver_->mode()!=terminal_ftxui::Mode::Inline||
     !driver_->interactive())return;
  const auto& view=state_.viewport;
  const auto rows=terminal_ftxui::advanceInlineScrollback(state_,
    ftxui::Dimensions{view.columns,view.rows},scrollbackCursor_,true);
  driver_->commitScrollback(rows);
}
void InteractionController::applyStream(StreamInput input){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Stream event requires owner thread");
  applyStreamMessage(state_,input,imports_.presentation);
  commitInlineScrollback();
  requestRedraw();
}
void InteractionController::setInput(std::string text,
                                     std::size_t caretUtf16){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Input update requires owner thread");
  state_.input.text=std::move(text);
  state_.input.caretUtf16=caretUtf16;
  ++state_.revision;
  requestRedraw();
}
void InteractionController::scroll(std::ptrdiff_t deltaRows){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Scroll requires owner thread");
  const auto layout=terminal_ftxui::layoutPresentation(state_,
    state_.viewport.columns,state_.viewport.rows);
  scrollViewport(state_,deltaRows,layout.transcriptRowCount,
                 layout.bodyHeight);
  requestRedraw();
}
void InteractionController::syncFocus(){
  const bool focused=terminalFocused_&&poetryDesiredFocus_;
  if(state_.input.focused==focused)return;
  state_.input.focused=focused;
  ++state_.revision;
  requestRedraw();
}
void InteractionController::onRawInput(std::string_view bytes){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Raw input requires owner thread");
  auto parsed=keyboard_input::parseMultipleKeypresses(parser_,
    keyboard_input::Input{Json(std::string(bytes))});
  parser_=std::move(parsed.state);
  for(const auto& item:parsed.keys)onParsedInput(item);
}
void InteractionController::onParsedInput(
    const keyboard_input::ParsedInput& item){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Parsed input requires owner thread");
  if(const auto response=std::get_if<keyboard_input::ParsedResponse>(&item)){
    imports_.onTerminalResponse(response->response);
    return;
  }
  if(const auto mouse=std::get_if<keyboard_input::MouseRef>(&item)){
    imports_.onMouse(*mouse);
    return;
  }
  onKey(std::get<keyboard_input::KeyRef>(item));
}
void InteractionController::onKey(const keyboard_input::KeyRef& key){
  if(key->sequence&&*key->sequence==termio::FOCUS_IN){
    terminalFocused_=true;syncFocus();imports_.onTerminalFocus(true);return;
  }
  if(key->sequence&&*key->sequence==termio::FOCUS_OUT){
    terminalFocused_=false;syncFocus();imports_.onTerminalFocus(false);return;
  }
  if(!terminalFocused_){terminalFocused_=true;syncFocus();}
  if(isName(key,"z")&&key->ctrl){
    driver_->withRestoredIO(imports_.onSuspend);
    return;
  }
  if(commands_->hasActionableUi()){
    onModalKey(key);
    return;
  }
  if(isName(key,"pageup")){scroll(-std::max(1,state_.viewport.rows/2));return;}
  if(isName(key,"pagedown")){scroll(std::max(1,state_.viewport.rows/2));return;}
  imports_.editPoetryKey(key,state_.input);
  ++state_.revision;
  requestRedraw();
}
void InteractionController::onModalKey(const keyboard_input::KeyRef& key){
  const auto& request=commands_->visibleUi();
  if(!request)throw std::logic_error("Active modal missing view");
  if(isName(key,"escape")||(isName(key,"c")&&key->ctrl)){
    commands_->dispatchAction({UiAction::Kind::Cancel,0,{}});return;
  }
  if(isName(key,"return")||isName(key,"enter")){
    commands_->dispatchAction({UiAction::Kind::Activate,
      request->selectedIndex,{}});return;
  }
  if(isName(key,"up")||isName(key,"down")){
    if(request->choices.empty())return;
    const auto index=isName(key,"up")?
      (request->selectedIndex==0?request->choices.size()-1:
        request->selectedIndex-1):
      (request->selectedIndex+1)%request->choices.size();
    commands_->dispatchAction({UiAction::Kind::MoveSelection,index,{}});
    return;
  }
  if(key->sequence)
    commands_->dispatchAction({UiAction::Kind::TextChange,0,*key->sequence});
}
} // namespace batbox::interactive
