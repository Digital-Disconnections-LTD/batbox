#include "interactive/conversation_shell/Shell.hpp"
#include "interactive/text_input/TextEditorIndigenous.hpp"
#include "interactive/TranscriptRows.hpp"
#include "runtime/JsonValue.hpp"
#include <cctype>
#include <stdexcept>

namespace batbox::interactive::conversation_shell {
namespace {
template<class T>void need(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Conversation shell binding required: ")+name);
}
}
Shell::Shell(Imports imports):imports_(std::move(imports)),
    baseMode_(imports_.driverOptions.mode){
  if(!imports_.owner.isOwnerThread())
    throw std::logic_error("Conversation shell requires owner thread");
  need(imports_.submitServices,"submitServices");
  need(imports_.makeSubmitRequest,"makeSubmitRequest");
  need(imports_.listSettings,"listSettings");
  need(imports_.surfaceOptions,"surfaceOptions");
  need(imports_.messageImports,"messageImports");
  need(imports_.decisionImports,"decisionImports");
  need(imports_.projectionServices,"projectionServices");
  need(imports_.renderStreamingMarkdown,"renderStreamingMarkdown");
  need(imports_.chrome.renderHeader,"renderHeader");
  need(imports_.chrome.renderFooter,"renderFooter");
  need(imports_.chrome.renderModal,"renderModal");
  need(imports_.chrome.renderPill,"renderPill");
  need(imports_.chrome.renderLogo,"renderLogo");
  need(imports_.chrome.renderDivider,"renderDivider");
  need(imports_.input.onTerminalResponse,"onTerminalResponse");
  need(imports_.input.onMouse,"onMouse");
  need(imports_.input.onTerminalFocus,"onTerminalFocus");
  need(imports_.input.onSuspend,"onSuspend");
  need(imports_.input.onQueueBlocked,"onQueueBlocked");
  need(imports_.input.onCommandSettled,"onCommandSettled");
  need(imports_.input.onCommandError,"onCommandError");
  need(imports_.input.writePlain,"writePlain");
  need(imports_.input.onExit,"onExit");
  need(imports_.onRepin,"onRepin");
  need(imports_.getSubmitCount,"getSubmitCount");
  need(imports_.copyMessageText,"copyMessageText");
  need(imports_.editMessage,"editMessage");
  need(imports_.nowMs,"nowMs");
  need(imports_.logForDebugging,"logForDebugging");
  need(imports_.terminalProgressEnabled,"terminalProgressEnabled");
  need(imports_.setTerminalProgress,"setTerminalProgress");
  need(imports_.onDetachedError,"onDetachedError");
}
Shell::~Shell(){
  incompleteEscapeTimer_.cancel();
  copyScrollTimer_.cancel();
  commandUiSwapTimer_.cancel();
  activityPulseTimer_.cancel();
  welcomeTimer_.cancel();
  try{stop();}catch(...){try{reportDetached(std::current_exception());}catch(...){}}
}
std::shared_ptr<Shell> Shell::create(Imports imports){
  auto self=std::shared_ptr<Shell>(new Shell(std::move(imports)));
  self->initialize();return self;
}
void Shell::attachCommandUi(std::shared_ptr<CommandUi> commandUi){
  if(!imports_.owner.isOwnerThread())
    throw std::logic_error("Command UI attachment requires shell owner");
  if(!commandUi)throw std::invalid_argument("Command UI required");
  if(started_)throw std::logic_error("Attach command UI before shell start");
  commandUiSwapTimer_.cancel();
  pendingCommandUi_.reset();
  commands_=std::move(commandUi);
}
void Shell::replaceCommandUi(std::shared_ptr<CommandUi> commandUi){
  if(!imports_.owner.isOwnerThread())
    throw std::logic_error("Command UI replacement requires shell owner");
  if(!commandUi)throw std::invalid_argument("Replacement command UI required");
  if(commandUi==commands_)return;
  if(commandUi->busy()||commandUi->hasActionableUi())
    throw std::logic_error("Replacement command UI must be idle");
  pendingCommandUi_=std::move(commandUi);
  finishCommandUiReplacement();
}
void Shell::finishCommandUiReplacement(){
  if(!pendingCommandUi_)return;
  if(started_&&commands_&&
     (commands_->busy()||commands_->hasActionableUi())){
    if(commandUiSwapTimer_.valid())return;
    auto weak=weak_from_this();
    commandUiSwapTimer_=imports_.owner.setTimeout(10,[weak]{
      if(auto self=weak.lock()){
        self->commandUiSwapTimer_={};
        self->finishCommandUiReplacement();
      }
    });
    return;
  }
  commandUiSwapTimer_.cancel();commandUiSwapTimer_={};
  commands_=std::move(pendingCommandUi_);
  requestRedraw();
}
void Shell::setCommandFocus(bool focused){
  poetryDesiredFocus_=focused;
  syncFocus();
}
void Shell::reportDetached(std::exception_ptr error)const{
  if(error)imports_.onDetachedError(error);
}
void Shell::initialize(){
  auto weak=weak_from_this();
  retained_=message_view::RetainedViews::create(imports_.owner,imports_.nowMs,
    [weak]{if(auto self=weak.lock())self->requestRedraw();});
  viewport_=viewport::Controller::create(
    [weak]{if(auto self=weak.lock())self->requestRedraw();});
  transcript_navigation::Services search(imports_.owner,*viewport_);
  search.extractSearchText=[weak](const RenderItem& item){
    auto self=weak.lock();if(!self||!self->prepared_)
      throw std::logic_error("Search index has no prepared transcript");
    return self->searchText_.extract(item.message,self->prepared_->lookups,
      self->currentGadgets_);
  };
  search.scanItem=[weak](std::size_t index,const std::string& query)
      ->std::optional<std::vector<terminal_render::MatchPosition>>{
    if(auto self=weak.lock())return self->viewport_->scanItem(index,query);
    return {};
  };
  search.setPositions=[weak](std::optional<transcript_navigation::PositionOverlay> value){
    if(auto self=weak.lock()){
      self->positions_=std::move(value);self->requestRedraw();
    }
  };
  search.onMatchesChanged=[weak](std::size_t count,std::size_t current){
    if(auto self=weak.lock()){
      self->searchCount_=count;self->searchCurrent_=current;
      self->requestRedraw();
    }
  };
  search.setStickyPoetry=[weak](transcript_navigation::StickyPoetry value){
    if(auto self=weak.lock()){
      self->sticky_=std::move(value);self->requestRedraw();
    }
  };
  search.scrollToMounted=[weak](std::size_t index,double offset){
    if(auto self=weak.lock())return self->viewport_->scrollToMounted(index,offset);
    return false;
  };
  search.performanceNow=imports_.nowMs;
  search.logForDebugging=imports_.logForDebugging;
  navigation_=std::make_shared<transcript_navigation::Navigation>(std::move(search));
  message_actions::Services action(imports_.owner);
  action.itemHeight=[weak](std::size_t index)->std::optional<double>{
    if(auto self=weak.lock())return self->viewport_->ranges().itemHeight(index);
    return {};
  };
  action.scrollToBottom=[weak]{if(auto self=weak.lock())self->jumpToBottom();};
  action.onCursorChanged=[weak](std::optional<message_actions::Cursor> value){
    if(auto self=weak.lock()){
      self->cursor_=std::move(value);
      if(self->cursor_){
        const auto index=self->actions_->selectedIndex();
        if(index>=0)self->viewport_->scrollToIndex(static_cast<std::size_t>(index));
      }
      self->requestRedraw();
    }
  };
  action.copy=imports_.copyMessageText;
  action.edit=[weak](MessageRef message){
    auto self=weak.lock();
    if(!self)throw std::runtime_error("Conversation shell closed");
    if(!self->batConversation_)return self->imports_.editMessage(std::move(message));
    event_loop::Deferred<event_loop::Unit> result(self->imports_.owner);
    result.reject(std::make_exception_ptr(std::runtime_error(
      "Use a follow-up to continue this Bat conversation")));
    return result;
  };
  action.onDetachedError=imports_.onDetachedError;
  actions_=std::make_unique<message_actions::MessageActionsController>(std::move(action));
  CommandUi::Callbacks commands;
  commands.onUiChanged=[weak](std::optional<UiRequest> value){
    if(auto self=weak.lock()){
      self->imports_.presentation.modal=std::move(value);
      ++self->imports_.presentation.revision;self->requestRedraw();
    }
  };
  commands.onSettled=[weak](const CommandDescriptor& command,
      const CommandCompletion& completion,bool failed){
    if(auto self=weak.lock())self->imports_.input.onCommandSettled(command,completion,failed);
  };
  commands.onError=[weak](const std::string& error){
    if(auto self=weak.lock())self->imports_.input.onCommandError(error);
  };
  commands.onQueueBlocked=[weak](bool blocked){
    if(auto self=weak.lock())self->imports_.input.onQueueBlocked(blocked);
  };
  commands.onPoetryFocus=[weak](bool focus){
    if(auto self=weak.lock()){
      self->poetryDesiredFocus_=focus;self->syncFocus();
    }
  };
  commands_=std::make_shared<CommandUi>(imports_.owner,std::move(commands));
  if(imports_.history){
    historyNavigator_=std::make_shared<application_history::Navigator>(
      application_history::Navigator::Inputs{
        imports_.history,imports_.presentation.input,
        [weak]{if(auto self=weak.lock())self->requestRedraw();},
        [weak](const std::string& text){
          if(auto self=weak.lock())if(self->imports_.editorCallbacks.addNotification)
            self->imports_.editorCallbacks.addNotification(
              {"search-history-hint",text,"immediate",2500});
        },
        imports_.onDetachedError});
  }
  auto editorCallbacks=imports_.editorCallbacks;
  const auto previousFilter=editorCallbacks.inputFilter;
  editorCallbacks.inputFilter=[weak,previousFilter](
      const std::string& input,const terminal_events::Key& key){
    auto filtered=previousFilter?previousFilter(input,key):input;
    if(auto self=weak.lock()){
      if(!self->pendingImageSpace_)return filtered;
      self->pendingImageSpace_=false;
      const bool control=key.ctrl||key.meta||key.escape||key.returnKey||
        key.tab||key.backspace||key.deleteKey||key.upArrow||key.downArrow||
        key.leftArrow||key.rightArrow||key.pageUp||key.pageDown||
        key.home||key.end;
      if(!control&&!filtered.empty()&&
          !std::isspace(static_cast<unsigned char>(filtered.front()))&&
          filtered.front()!='\x1b')return std::string(" ")+filtered;
    }
    return filtered;
  };
  if(historyNavigator_){
    editorCallbacks.onHistoryUp=[weak]{if(auto self=weak.lock())
      if(!self->batConversation_)self->historyNavigator_->up();};
    editorCallbacks.onHistoryDown=[weak]{if(auto self=weak.lock())
      if(!self->batConversation_)(void)self->historyNavigator_->down();};
    editorCallbacks.onHistoryReset=[weak]{if(auto self=weak.lock())
      if(!self->batConversation_)self->historyNavigator_->reset();};
    editorCallbacks.addToHistory=[weak](const std::string& text){
      if(auto self=weak.lock())if(!self->batConversation_)
        self->imports_.history->record(text,self->imports_.presentation.input.pastedContents);
    };
  }
  auto changed=editorCallbacks.onChange;
  editorCallbacks.onChange=[weak,changed](const std::string& text){
    if(auto self=weak.lock()){
      if(changed&&!self->batConversation_)changed(text);
      ++self->presentation().revision;
      self->requestRedraw();
    }
  };
  auto offset=editorCallbacks.onOffsetChange;
  editorCallbacks.onOffsetChange=[weak,offset](std::size_t value){
    if(auto self=weak.lock()){
      if(offset&&!self->batConversation_)offset(value);
      self->requestRedraw();
    }
  };
  editorCallbacks.onSubmit=[weak](const std::string& text){
    if(auto self=weak.lock())self->submitPoetry(text);
  };
  editorCallbacks.onExit=[weak]{if(auto self=weak.lock())self->imports_.input.onExit();};
  editorCallbacks=text_input::indigenousEditorCallbacks(std::move(editorCallbacks));
  editor_=std::make_unique<text_input::TextEditor>(imports_.owner,killRing_,
    imports_.editorMetrics,imports_.editorOptions,std::move(editorCallbacks));
  searchInput_.poetry="/";
  auto searchCallbacks=imports_.editorCallbacks;
  searchCallbacks.onChange=[weak](const std::string& text){
    if(auto self=weak.lock())self->setSearchQuery(text);
  };
  searchCallbacks.onOffsetChange=[weak](std::size_t){
    if(auto self=weak.lock())self->requestRedraw();
  };
  searchCallbacks.onSubmit=[weak](const std::string&){
    if(auto self=weak.lock())self->closeSearch(true);
  };
  searchCallbacks.onExit=[weak]{
    if(auto self=weak.lock())self->closeSearch(false);
  };
  searchCallbacks.onHistoryUp={};searchCallbacks.onHistoryDown={};
  searchCallbacks.onHistoryReset={};searchCallbacks.addToHistory={};
  searchCallbacks=text_input::indigenousEditorCallbacks(std::move(searchCallbacks));
  auto searchOptions=imports_.editorOptions;
  searchOptions.multiline=false;
  searchOptions.disableEscapeDoublePress=true;
  searchEditor_=std::make_unique<text_input::TextEditor>(imports_.owner,
    killRing_,imports_.editorMetrics,std::move(searchOptions),
    std::move(searchCallbacks));
  terminal_ftxui::Callbacks callbacks;
  callbacks.render=[weak]{
    auto self=weak.lock();if(!self)throw std::logic_error("Conversation shell expired");
    return self->render();
  };
  callbacks.onRawInputBytes=[weak](std::string_view bytes){
    if(auto self=weak.lock())self->onRawInput(bytes);
  };
  callbacks.consumeMouseEvent=[weak](ftxui::Event event){
    auto self=weak.lock();if(!self||!event.is_mouse())return false;
    if(self->copyView_)return self->onCopyMouse(event.mouse());
    const auto row=event.mouse().y;
    return (self->pillScreenRow_&&row==*self->pillScreenRow_)||
      (self->stickyScreenRow_&&row==*self->stickyScreenRow_)||
      (self->missionScreenRow_&&row>=*self->missionScreenRow_&&
        row<=*self->missionScreenRow_+static_cast<int>(self->footerMissionIds_.size()));
  };
  callbacks.copySelectedText=[weak](const std::string& text){
    if(auto self=weak.lock())self->imports_.copyMessageText(text);
  };
  callbacks.onResize=[weak](ftxui::Dimensions size){
    if(auto self=weak.lock())self->onResize(size);
  };
  callbacks.renderPlain=[weak]{
    auto self=weak.lock();if(!self)throw std::logic_error("Conversation shell expired");
    return terminal_ftxui::drainPresentationPlain(self->imports_.presentation,
      self->plainCursor_);
  };
  callbacks.writePlain=[weak](std::string_view text){
    if(auto self=weak.lock())self->imports_.input.writePlain(text);
  };
  callbacks.onExit=[weak]{if(auto self=weak.lock())self->imports_.input.onExit();};
  callbacks.onFrameCommitted=[weak]{if(auto self=weak.lock())self->committed();};
  driver_=std::make_shared<terminal_ftxui::Driver>(imports_.owner,
    imports_.driverOptions,std::move(callbacks));
}
void Shell::bindRuntime(std::shared_ptr<repl_runtime::Runtime> runtime){
  if(started_)throw std::logic_error("Runtime binding after shell start");
  if(!runtime)throw std::invalid_argument("Live REPL runtime required");
  runtime_=std::move(runtime);
}
void Shell::start(){
  if(started_)return;
  if(!runtime_)throw std::logic_error("Conversation runtime binding required");
  welcomeFrame_=!welcomeStarted_&&imports_.driverOptions.interactive&&
    driver_->mode()==terminal_ftxui::Mode::Fullscreen?0:welcomeAnimationFrames;
  welcomeStarted_=true;
  welcomeRendered_=false;
  driver_->start();
  try{runtime_->start();started_=true;}
  catch(...){driver_->stop();throw;}
  syncActivityPulse();
  syncWelcomeAnimation();
}
void Shell::stop(){
  remembering_=false;
  activityPulseTimer_.cancel();activityPulseTimer_={};activityFrame_=0;
  welcomeTimer_.cancel();welcomeTimer_={};welcomeFrame_=welcomeAnimationFrames;
  copyScrollTimer_.cancel();copyScrollTimer_={};copyView_.reset();
  if(!started_)return;
  started_=false;
  ++batOpenGeneration_;
  if(batConversation_&&imports_.releaseBatMission)
    imports_.releaseBatMission(batConversation_->id);
  batConversation_.reset();
  transcriptExport_.reset();
  pastePending_=false;pendingImageSpace_=false;bufferedPasteKeys_.clear();
  if(historyNavigator_)historyNavigator_->reset();
  incompleteEscapeTimer_.cancel();
  commandUiSwapTimer_.cancel();commandUiSwapTimer_={};
  pendingCommandUi_.reset();
  try{runtime_->stop();}catch(...){reportDetached(std::current_exception());}
  driver_->stop();
  retained_->unmountAll();
  surfaceState_.unmounted(imports_.setTerminalProgress);
}
void Shell::requestRedraw(){
  syncActivityPulse();
  syncWelcomeAnimation();
  if(driver_)driver_->requestRedraw();
}
void Shell::setRemembering(bool active){
  if(!imports_.owner.isOwnerThread())
    throw std::logic_error("Project recovery UI requires shell owner");
  if(remembering_==active)return;
  remembering_=active;requestRedraw();
}
void Shell::syncActivityPulse(){
  const bool active=started_&&driver_&&driver_->interactive()&&
    driver_->mode()==terminal_ftxui::Mode::Fullscreen&&
    !driver_->suspended()&&terminalFocused_&&
    (remembering_||presentation().loading);
  if(!active){
    activityPulseTimer_.cancel();activityPulseTimer_={};activityFrame_=0;
    return;
  }
  if(activityPulseTimer_.valid())return;
  auto weak=weak_from_this();
  // A single unreferenced owner-loop timer paints eight frames per second
  // while busy. It neither keeps a flight alive nor polls an idle model.
  activityPulseTimer_=imports_.owner.setTimeout(125,[weak]{
    if(auto self=weak.lock()){
      self->activityPulseTimer_={};
      self->activityFrame_=(self->activityFrame_+1)%16;
      self->requestRedraw();
    }
  },false);
}
void Shell::syncWelcomeAnimation(){
  const bool active=started_&&driver_&&driver_->interactive()&&
    driver_->mode()==terminal_ftxui::Mode::Fullscreen&&
    !driver_->suspended()&&terminalFocused_&&
    !transcript_&&!batConversation_;
  if(!active){
    welcomeTimer_.cancel();welcomeTimer_={};
    if(started_)welcomeFrame_=welcomeAnimationFrames;
    return;
  }
  // The first interactive paint may occur after start() returns.
  if(!welcomeRendered_)return;
  if(!welcomeVisible_){
    welcomeTimer_.cancel();welcomeTimer_={};welcomeFrame_=welcomeAnimationFrames;
    return;
  }
  if(welcomeFrame_>=welcomeAnimationFrames||welcomeTimer_.valid())return;
  auto weak=weak_from_this();
  // The entrance ends after twelve frames; settled art needs no timer.
  welcomeTimer_=imports_.owner.setTimeout(100,[weak]{
    if(auto self=weak.lock()){
      self->welcomeTimer_={};
      ++self->welcomeFrame_;
      self->requestRedraw();
    }
  },false);
}
void Shell::resetHistory(){
  if(historyNavigator_)historyNavigator_->reset();
}
void Shell::submitPoetry(const std::string& text){
  if(submitBatPoetry(text))return;
  const std::string submittedText=text;
  suggestions_.clear();++completionGeneration_;
  auto& input=presentation().input;
  auto request=imports_.makeSubmitRequest(submittedText,input);
  if(!request.input)request.input=submittedText;
  if(imports_.history&&!batConversation_)imports_.history->record(submittedText,input.pastedContents);
  if(historyNavigator_)historyNavigator_->reset();
  // The input handler can complete a local UI command without running
  // the normal poetry-clear branch. The indigenous editor reads this shared state.
  input.text.clear();input.caretUtf16=0;
  ++presentation().revision;requestRedraw();
  auto result=submit::handlePoetrySubmit(std::move(request),imports_.submitServices);
  auto weak=weak_from_this();
  const auto submittedBat=activeBatId();
  result.then([weak,text=submittedText,submittedBat](const event_loop::Unit*,const std::exception_ptr& error){
    if(auto self=weak.lock()){
      if(error&&self->activeBatId()==submittedBat&&self->presentation().input.text.empty()){
        self->presentation().input.text=text;
        self->presentation().input.caretUtf16=value::stringLength(text);
        ++self->presentation().revision;self->requestRedraw();
      }
      self->reportDetached(error);
    }
  });
}
void Shell::applyStream(StreamInput input){
  if(!imports_.owner.isOwnerThread())throw std::logic_error("Stream owner required");
  if(input.event){
    auto& state=imports_.presentation;
    const auto& type=value::property(*input.event,"type");
    if(type=="stream_request_start"){
      streamingThinkingIndex_.reset();state.streamingThinking.reset();
    }else if(type=="stream_event"){
      const auto& event=value::property(*input.event,"event");
      const auto& eventType=value::property(event,"type");
      const auto& block=value::property(event,"content_block");
      const auto& delta=value::property(event,"delta");
      if(eventType=="content_block_start"&&value::property(block,"type")=="thinking"){
        const auto& initial=value::property(block,"thinking");
        state.streamingThinking=StreamingThinking{
          initial.is_string()?initial.get<std::string>():"",true,{}};
        streamingThinkingIndex_=value::property(event,"index");
      }else if(eventType=="content_block_delta"&&streamingThinkingIndex_&&
          *streamingThinkingIndex_==value::property(event,"index")&&
          value::property(delta,"type")=="thinking_delta"){
        const auto& text=value::property(delta,"thinking");
        if(text.is_string()&&state.streamingThinking)
          state.streamingThinking->thinking+=text.get<std::string>();
      }else if((eventType=="content_block_stop"&&streamingThinkingIndex_&&
          *streamingThinkingIndex_==value::property(event,"index"))||
          eventType=="message_stop"){
        if(state.streamingThinking&&state.streamingThinking->isStreaming){
          state.streamingThinking->isStreaming=false;
          state.streamingThinking->streamingEndedAt=imports_.nowMs();
        }
        streamingThinkingIndex_.reset();
      }
    }
  }
  applyStreamMessage(imports_.presentation,input,imports_.input.presentation);
  observeLastMessage();
  commitInlineScrollback();requestRedraw();
}
void Shell::observeLastMessage(){
  const auto& messages=imports_.presentation.messages;
  const auto current=messages.empty()?MessageRef{}:messages.back();
  if(current==lastObservedMessage_)return;
  lastObservedMessage_=current;
}
void Shell::messagesChanged(){
  if(!imports_.owner.isOwnerThread())throw std::logic_error("Message owner required");
  rebuildTranscriptRows(imports_.presentation);
  ++imports_.presentation.revision;
  observeLastMessage();
  commitInlineScrollback();requestRedraw();
}
void Shell::syncFocus(){
  syncActivityPulse();
  const auto focused=terminalFocused_&&poetryDesiredFocus_;
  if(presentation().input.focused==focused)return;
  presentation().input.focused=focused;
  ++presentation().revision;requestRedraw();
}
} // namespace batbox::interactive::conversation_shell
