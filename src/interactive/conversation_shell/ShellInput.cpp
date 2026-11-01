#include "interactive/conversation_shell/Shell.hpp"
#include "termio/Csi.hpp"
#include "history/References.hpp"
#include "npm_wrap_ansi/Internal.hpp"
#include "application_editor/PoetryEditor.hpp"
#include "application_editor/IndigenousProcess.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <set>

namespace batbox::interactive::conversation_shell {
namespace {
bool named(const keyboard_input::KeyRef& key,const char* name){
  return key->name&&*key->name==name;
}
bool plain(const keyboard_input::KeyRef& key){
  return !key->ctrl&&!key->meta&&!key->super;
}
bool repeated(const std::string& value,char letter){
  return !value.empty()&&std::all_of(value.begin(),value.end(),
    [letter](char c){return c==letter;});
}
bool maybeImagePathPaste(const std::string& text){
  std::string lower=text;
  std::transform(lower.begin(),lower.end(),lower.begin(),
    [](unsigned char c){return static_cast<char>(std::tolower(c));});
  return lower.find(".png")!=std::string::npos||
    lower.find(".jpg")!=std::string::npos||
    lower.find(".jpeg")!=std::string::npos||
    lower.find(".gif")!=std::string::npos||
    lower.find(".webp")!=std::string::npos;
}
int maxPasteId(const PresentationState& state){
  int maximum=0;
  const auto scan=[&maximum](const std::string& text){
    for(const auto& ref:history::parseReferences(text))
      maximum=std::max(maximum,static_cast<int>(ref.id));
  };
  scan(state.input.text);
  if(state.input.pastedContents.is_object())
    for(const auto& item:state.input.pastedContents.items()){
      const auto& id=value::property(item.value(),"id");
      if(id.is_number_integer())maximum=std::max(maximum,id.get<int>());
    }
  for(const auto& message:state.messages){
    if(!message)continue;
    const auto& ids=value::property(*message,"imagePasteIds");
    if(ids.is_array())for(const auto& id:ids)
      if(id.is_number_integer())maximum=std::max(maximum,id.get<int>());
    const auto& content=value::property(value::property(*message,"message"),"content");
    if(content.is_string())scan(content.get<std::string>());
    else if(content.is_array())for(const auto& block:content){
      const auto& text=value::property(block,"text");
      if(text.is_string())scan(text.get<std::string>());
    }
  }
  return maximum;
}
void prunePastedImages(InputState& input){
  if(!input.pastedContents.is_object()||input.pastedContents.empty())return;
  std::set<int> referenced;
  for(const auto& ref:history::parseReferences(input.text))
    referenced.insert(static_cast<int>(ref.id));
  for(auto it=input.pastedContents.begin();it!=input.pastedContents.end();){
    const auto& content=it.value();
    const auto& id=value::property(content,"id");
    if(value::property(content,"type")=="image"&&id.is_number_integer()&&
        !referenced.contains(id.get<int>()))it=input.pastedContents.erase(it);
    else ++it;
  }
}
}
void Shell::onRawInput(std::string_view bytes){
  if(!imports_.owner.isOwnerThread())throw std::logic_error("Input owner required");
  incompleteEscapeTimer_.cancel();incompleteEscapeTimer_={};
  auto parsed=keyboard_input::parseMultipleKeypresses(parser_,
    keyboard_input::Input{Json(std::string(bytes))});
  parser_=std::move(parsed.state);
  for(const auto& item:parsed.keys)onParsedInput(item);
  if(!parser_.incomplete.empty()){
    const auto delay=parser_.mode=="IN_PASTE"?500.0:50.0;
    auto weak=weak_from_this();
    incompleteEscapeTimer_=imports_.owner.setTimeout(delay,[weak]{
      if(auto self=weak.lock()){
        self->incompleteEscapeTimer_={};
        if(self->parser_.incomplete.empty())return;
        auto flushed=keyboard_input::parseMultipleKeypresses(self->parser_,
          keyboard_input::Input{Json(nullptr)});
        self->parser_=std::move(flushed.state);
        for(const auto& item:flushed.keys)self->onParsedInput(item);
      }
    });
  }
}
void Shell::onParsedInput(const keyboard_input::ParsedInput& input){
  if(const auto response=std::get_if<keyboard_input::ParsedResponse>(&input)){
    imports_.input.onTerminalResponse(response->response);return;
  }
  if(const auto mouse=std::get_if<keyboard_input::MouseRef>(&input)){
    if(copyView_)return;
    const auto row=static_cast<int>((*mouse)->row)-1;
    if((*mouse)->action=="press"&&(*mouse)->button==0){
      if(stickyScreenRow_&&row==*stickyScreenRow_){
        navigation_->clickStickyPoetry();return;
      }
      if(pillScreenRow_&&row==*pillScreenRow_){jumpToBottom();return;}
      if(missionScreenRow_&&row>=*missionScreenRow_&&
          row<=*missionScreenRow_+static_cast<int>(footerMissionIds_.size())&&
          imports_.openBackgroundMission){
        const auto index=row-*missionScreenRow_-1;
        imports_.openBackgroundMission(index<0?std::string{}:
          footerMissionIds_[static_cast<std::size_t>(index)]);return;
      }
    }
    imports_.input.onMouse(*mouse);return;
  }
  onKey(std::get<keyboard_input::KeyRef>(input));
}
void Shell::onKey(const keyboard_input::KeyRef& key){
  if(!key)return;
  if(copyView_){
    if(named(key,"escape")||(named(key,"q")&&plain(key))){
      copyScrollTimer_.cancel();copyScrollTimer_={};copyView_.reset();
      requestRedraw();return;
    }
    if(named(key,"return")||named(key,"enter")||
        (named(key,"c")&&key->ctrl)){
      const auto text=copyView_->selectedText();
      if(!text.empty())imports_.copyMessageText(text);
      return;
    }
    if(named(key,"wheelup")||named(key,"up")){copyView_->scrollBy(-3);requestRedraw();return;}
    if(named(key,"wheeldown")||named(key,"down")){copyView_->scrollBy(3);requestRedraw();return;}
    if(named(key,"pageup")){copyView_->scrollBy(-std::max(1,presentation().viewport.rows-2));requestRedraw();return;}
    if(named(key,"pagedown")){copyView_->scrollBy(std::max(1,presentation().viewport.rows-2));requestRedraw();return;}
    if(named(key,"home")){copyView_->scrollBy(-static_cast<int>(copyView_->top()));requestRedraw();return;}
    return;
  }
  if(named(key,"c")&&(key->ctrl||key->super)){
    const auto selected=driver_->selectedText();
    if(!selected.empty()){imports_.copyMessageText(selected);return;}
  }
  if(pastePending_){bufferedPasteKeys_.push_back(key);return;}
  if(key->sequence&&*key->sequence==termio::FOCUS_IN){
    terminalFocused_=true;syncFocus();
    imports_.input.onTerminalFocus(true);return;
  }
  if(key->sequence&&*key->sequence==termio::FOCUS_OUT){
    terminalFocused_=false;syncFocus();
    imports_.input.onTerminalFocus(false);return;
  }
  if(!terminalFocused_){terminalFocused_=true;syncFocus();}
  if(named(key,"z")&&key->ctrl){
    driver_->withRestoredIO(imports_.input.onSuspend);return;
  }
  if(commands_->hasActionableUi()){onModalKey(key);return;}
  if(searchOpen_){onSearchKey(key);return;}
  if(batConversation_&&!cursor_&&!transcript_){
    if((named(key,"left")&&key->meta)||
        (named(key,"escape")&&presentation().input.text.empty())){
      returnToLeader();return;
    }
    if(named(key,"c")&&key->ctrl){
      if(!presentation().input.text.empty()){
        presentation().input.text.clear();presentation().input.caretUtf16=0;
      }else if(imports_.stopBatMission){
        try{imports_.stopBatMission(batConversation_->id);}
        catch(...){reportDetached(std::current_exception());}
      }
      requestRedraw();return;
    }
  }
  if(named(key,"end")&&key->ctrl){jumpToBottom();return;}
  if(!batConversation_&&named(key,"b")&&key->ctrl&&imports_.backgroundForegroundMissions){
    const bool changed=imports_.backgroundForegroundMissions();
    if(changed)requestRedraw();
    return;
  }
  if(!batConversation_&&historyNavigator_&&!transcript_&&named(key,"r")&&key->ctrl){
    suggestions_.clear();++completionGeneration_;
    if(!batConversation_&&historyNavigator_->searching())historyNavigator_->nextSearchMatch();
    else historyNavigator_->startSearch();
    requestRedraw();return;
  }
  if(!batConversation_&&historyNavigator_&&historyNavigator_->searching()){
    if(named(key,"escape")||named(key,"tab")){
      historyNavigator_->acceptSearch();requestRedraw();return;
    }
    if(named(key,"c")&&key->ctrl){
      historyNavigator_->cancelSearch();requestRedraw();return;
    }
    if(named(key,"return")||named(key,"enter")){
      const auto chosen=historyNavigator_->executeSearch();
      if(chosen&&!value::trim(*chosen).empty())submitPoetry(*chosen);
      else requestRedraw();
      return;
    }
    if(named(key,"backspace")){
      const auto& query=historyNavigator_->searchQuery();
      if(query.empty())historyNavigator_->cancelSearch();
      else historyNavigator_->setSearchQuery(
        value::slice(query,0,value::stringLength(query)-1));
      requestRedraw();return;
    }
    if(key->sequence&&plain(key)&&!key->sequence->empty()&&
       static_cast<unsigned char>(key->sequence->front())>=0x20&&
       !named(key,"up")&&!named(key,"down")&&!named(key,"left")&&
       !named(key,"right"))
      historyNavigator_->setSearchQuery(
        historyNavigator_->searchQuery()+*key->sequence);
    requestRedraw();return;
  }
  if(!transcript_&&!suggestions_.empty()){
    if(named(key,"tab")){acceptCompletion(false);return;}
    if(named(key,"return")||named(key,"enter")){
      acceptCompletion(true);return;
    }
    if(named(key,"escape")){
      suggestions_.clear();++completionGeneration_;requestRedraw();return;
    }
    if(suggestions_.size()>1&&(named(key,"up")||named(key,"down"))){
      selectedSuggestion_=named(key,"up")?
        (selectedSuggestion_==0?suggestions_.size()-1:selectedSuggestion_-1):
        (selectedSuggestion_+1)%suggestions_.size();
      requestRedraw();return;
    }
  }
  if(transcript_&&driver_->mode()==terminal_ftxui::Mode::Fullscreen){
    if(key->sequence&&*key->sequence=="c"&&plain(key)&&
        presentation().viewport.rows>=3){
      copyView_=std::make_unique<terminal_ftxui::TranscriptCopy>(
        presentation(),presentation().viewport.columns,presentation().viewport.rows);
      requestRedraw();return;
    }
    if(key->sequence&&*key->sequence=="v"&&plain(key)){
      startTranscriptExport();return;
    }
    if((named(key,"q")&&plain(key))||named(key,"escape")||
       (named(key,"c")&&key->ctrl)){setTranscript(false);return;}
    if(key->sequence&&*key->sequence=="/"){
      openSearch();return;
    }
    if(key->sequence&&searchCount_>0&&
       (repeated(*key->sequence,'n')||repeated(*key->sequence,'N'))){
      for(const auto c:*key->sequence){
        if(c=='n')navigation_->nextMatch();
        else navigation_->prevMatch();
      }
      return;
    }
    if(named(key,"g")&&!key->shift&&plain(key)){
      viewport_->scrollTo(0);navigation_->disarmSearch();return;
    }
    if(named(key,"g")&&key->shift&&plain(key)){
      viewport_->scrollToBottom();navigation_->disarmSearch();return;
    }
  }
  if(cursor_){
    if(named(key,"escape")){actions_->dispatch("messageActions:escape");return;}
    if(named(key,"c")&&key->ctrl){actions_->dispatch("messageActions:ctrlc");return;}
    if(named(key,"up")){actions_->dispatch("messageActions:prev");return;}
    if(named(key,"down")){actions_->dispatch("messageActions:next");return;}
    if(named(key,"return")||named(key,"enter")){
      actions_->dispatch("messageActions:enter");return;
    }
    if(named(key,"c")&&plain(key)){
      actions_->dispatch("messageActions:c");return;
    }
    if(named(key,"p")&&plain(key)){
      actions_->dispatch("messageActions:p");return;
    }
  }
  if(named(key,"o")&&key->ctrl){setTranscript(!transcript_);return;}
  if(!transcript_&&!cursor_&&named(key,"g")&&key->ctrl){
    try{
      if(!application_editor::indigenous::externalEditor())return;
      application_editor::EditResult edited;
      driver_->withRestoredIO([&]{
        edited=application_editor::editPoetryInEditor(
          presentation().input.text,
          presentation().input.pastedContents);
      });
      if(edited.error&&imports_.editorCallbacks.addNotification)
        imports_.editorCallbacks.addNotification({"external-editor-error",
          *edited.error,"high",4000});
      if(edited.content&&*edited.content!=presentation().input.text){
        presentation().input.text=*edited.content;
        presentation().input.caretUtf16=value::stringLength(*edited.content);
        pendingImageSpace_=false;
        if(!batConversation_&&historyNavigator_)historyNavigator_->observeManualEdit();
        prunePastedImages(presentation().input);
        if(!batConversation_&&runtime_)runtime_->observeInputActivity(
          presentation().input.text,imports_.getSubmitCount());
        refreshCompletions();
        ++presentation().revision;requestRedraw();
      }
    }catch(const std::exception& issue){
      if(imports_.editorCallbacks.addNotification)
        imports_.editorCallbacks.addNotification({"external-editor-error",
          std::string("External editor failed: ")+issue.what(),"high",4000});
    }
    return;
  }
  if(named(key,"v")&&(key->ctrl||key->meta)&&imports_.getImageFromClipboard){
    auto pending=imports_.getImageFromClipboard();
    if(!imports_.owner.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Clipboard image Promise crossed owner loop");
    auto weak=weak_from_this();
    pending.then([weak](const Json* image,const std::exception_ptr& error){
      if(auto self=weak.lock()){
        if(!self->started_)return;
        if(error){self->reportDetached(error);return;}
        if(image)self->onClipboardImage(*image);
      }
    });return;
  }
  if(key->isPasted&&key->sequence&&
      imports_.readPastedImagePaths&&
      maybeImagePathPaste(*key->sequence)){
    auto pending=imports_.readPastedImagePaths(*key->sequence);
    if(!imports_.owner.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Pasted image Promise crossed owner loop");
    pastePending_=true;
    auto weak=weak_from_this();
    const auto original=*key->sequence;
    pending.then([weak,original](const Json* result,const std::exception_ptr& error){
      if(auto self=weak.lock()){
        if(!self->started_){self->bufferedPasteKeys_.clear();return;}
        self->pastePending_=false;
        if(error||!result){
          self->onPastedText(original);
          self->reportDetached(error);
        }else{
          const auto& images=value::property(*result,"images");
          if(images.is_array())for(const auto& image:images)
            self->onClipboardImage(image);
          const auto& remaining=value::property(*result,"text");
          if(remaining.is_string()&&!remaining.get<std::string>().empty())
            self->onPastedText(remaining.get<std::string>());
        }
        while(!self->pastePending_&&!self->bufferedPasteKeys_.empty()){
          auto next=self->bufferedPasteKeys_.front();
          self->bufferedPasteKeys_.pop_front();self->onKey(next);
        }
      }
    });return;
  }
#ifdef __APPLE__
  if(key->isPasted&&key->sequence&&key->sequence->empty()&&
      imports_.getImageFromClipboard){
    auto pending=imports_.getImageFromClipboard();
    auto weak=weak_from_this();
    pending.then([weak](const Json* image,const std::exception_ptr& error){
      if(auto self=weak.lock()){
        if(!self->started_)return;
        if(error)self->reportDetached(error);
        else if(image&&image->is_object())self->onClipboardImage(*image);
      }
    });return;
  }
#endif
  if(key->isPasted&&key->sequence){
    onPastedText(*key->sequence);return;
  }
  if(named(key,"wheelup")){scroll(-3);return;}
  if(named(key,"wheeldown")){scroll(3);return;}
  if(named(key,"pageup")){scroll(-std::max(1,presentation().viewport.rows/2));return;}
  if(named(key,"pagedown")){scroll(std::max(1,presentation().viewport.rows/2));return;}
  if(key->isPasted)pendingImageSpace_=false;
  const auto before=presentation().input.text;
  const auto caret=presentation().input.caretUtf16;
  editor_->onParsedKey(*key,presentation().input);
  if(before!=presentation().input.text){
    if(!batConversation_&&historyNavigator_)historyNavigator_->observeManualEdit();
    prunePastedImages(presentation().input);
  }
  if(before!=presentation().input.text&&!batConversation_&&runtime_)
    runtime_->observeInputActivity(presentation().input.text,
      imports_.getSubmitCount());
  if(before!=presentation().input.text||
     caret!=presentation().input.caretUtf16)
    refreshCompletions();
  ++presentation().revision;requestRedraw();
}
void Shell::refreshCompletions(){
  const auto generation=++completionGeneration_;
  if(!imports_.completion||transcript_||commands_->hasActionableUi()||
      presentation().input.text.empty()){
    suggestions_.clear();selectedSuggestion_=0;requestRedraw();return;
  }
  auto pending=imports_.completion->suggest(
    presentation().input.text,presentation().input.caretUtf16);
  if(!imports_.owner.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Completion Promise crossed owner loop");
  auto weak=weak_from_this();
  pending.then([weak,generation](
      const std::vector<application_completion::Suggestion>* result,
      const std::exception_ptr& error){
    if(auto self=weak.lock()){
      if(generation!=self->completionGeneration_)return;
      if(error){self->reportDetached(error);return;}
      self->suggestions_=result?*result:
        std::vector<application_completion::Suggestion>{};
      self->selectedSuggestion_=0;self->requestRedraw();
    }
  });
}
void Shell::acceptCompletion(bool execute){
  if(suggestions_.empty()||selectedSuggestion_>=suggestions_.size())return;
  const auto selected=suggestions_[selectedSuggestion_];
  auto& input=presentation().input;
  const auto length=value::stringLength(input.text);
  if(selected.startUtf16>selected.endUtf16||
      selected.endUtf16>length)return;
  const auto left=value::slice(input.text,0,selected.startUtf16);
  const auto right=value::slice(input.text,selected.endUtf16);
  input.text=left+selected.replacement+right;
  input.caretUtf16=selected.startUtf16+
    value::stringLength(selected.replacement);
  if(!batConversation_&&historyNavigator_)historyNavigator_->observeManualEdit();
  prunePastedImages(input);
  suggestions_.clear();selectedSuggestion_=0;++completionGeneration_;
  ++presentation().revision;requestRedraw();
  if(!batConversation_&&runtime_)runtime_->observeInputActivity(input.text,
    imports_.getSubmitCount());
  if(execute&&selected.executeWithoutArgs)
    submitPoetry(input.text);
  else refreshCompletions();
}
void Shell::onClipboardImage(const Json& image){
  if(!started_)return;
  if(!image.is_object()){
    if(imports_.editorCallbacks.addNotification)
      imports_.editorCallbacks.addNotification({"no-image-in-clipboard",
        "No image found in clipboard","immediate",1000});
    return;
  }
  const auto& base64=value::property(image,"base64");
  if(!base64.is_string()||base64.get<std::string>().empty())return;
  auto& input=presentation().input;
  // onImagePaste switches from Bash input mode to poetry mode before
  // inserting the image. The indigenous editor represents Bash mode with `!`.
  if(input.text.starts_with('!')){
    input.text.erase(input.text.begin());
    if(input.caretUtf16>0)--input.caretUtf16;
  }
  const int id=maxPasteId(presentation())+1;
  const auto& filename=value::property(image,"filename");
  Json content={{"id",id},{"type","image"},{"content",base64},
    {"mediaType",value::property(image,"mediaType")},
    {"filename",filename.is_string()?filename:Json("Pasted image")},
    {"dimensions",value::property(image,"dimensions")}};
  const auto& sourcePath=value::property(image,"sourcePath");
  if(sourcePath.is_string())content["sourcePath"]=sourcePath;
  input.pastedContents[std::to_string(id)]=content;
  const std::string left=value::slice(input.text,0,input.caretUtf16);
  const std::string right=value::slice(input.text,input.caretUtf16);
  const auto inserted=(pendingImageSpace_?" ":"")+history::formatImageRef(id);
  input.text=left+inserted+right;
  pendingImageSpace_=true;
  if(!batConversation_&&historyNavigator_)historyNavigator_->observeManualEdit();
  input.caretUtf16+=value::stringLength(inserted);
  if(imports_.onImagePasted)imports_.onImagePasted(content);
  if(!batConversation_&&runtime_)runtime_->observeInputActivity(input.text,imports_.getSubmitCount());
  refreshCompletions();
  ++presentation().revision;requestRedraw();
}
void Shell::onPastedText(const std::string& text){
  if(text.empty())return;
  pendingImageSpace_=false;
  auto cleaned=value::fromUtf16(npm_wrap_ansi::stripAnsi(value::utf16CodeUnits(text)));
  std::string normalized;normalized.reserve(cleaned.size());
  for(std::size_t i=0;i<cleaned.size();++i){
    const char c=cleaned[i];
    if(c=='\r'){
      normalized+='\n';
      if(i+1<cleaned.size()&&cleaned[i+1]=='\n')++i;
    }
    else if(c=='\t')normalized+="    ";
    else normalized+=c;
  }
  if(normalized.empty())return;
  const auto lines=history::getPastedTextRefNumLines(normalized);
  const int maxLines=std::clamp(presentation().viewport.rows-10,1,2);
  std::string inserted=normalized;
  if(value::stringLength(normalized)>800||
      static_cast<int>(lines)>maxLines){
    const int id=maxPasteId(presentation())+1;
    presentation().input.pastedContents[std::to_string(id)]=
      Json{{"id",id},{"type","text"},{"content",normalized}};
    const auto characters=std::count_if(normalized.begin(),normalized.end(),
      [](unsigned char c){return (c&0xc0)!=0x80;});
    inserted=history::formatPastedTextRef(id,lines,characters);
  }
  keyboard_input::ParsedKey key;
  key.name="";key.sequence=inserted;key.raw=inserted;key.isPasted=true;
  const auto before=presentation().input.text;
  editor_->onParsedKey(key,presentation().input);
  if(before!=presentation().input.text){
    if(!batConversation_&&historyNavigator_)historyNavigator_->observeManualEdit();
    prunePastedImages(presentation().input);
    if(!batConversation_&&runtime_)runtime_->observeInputActivity(
      presentation().input.text,imports_.getSubmitCount());
    refreshCompletions();
  }
  ++presentation().revision;requestRedraw();
}
void Shell::onModalKey(const keyboard_input::KeyRef& key){
  const auto& request=commands_->visibleUi();
  if(!request)throw std::logic_error("Active command has no view");
  if(named(key,"escape")||(named(key,"c")&&key->ctrl)){
    commands_->dispatchAction({UiAction::Kind::Cancel,0,{}});return;
  }
  if(named(key,"return")||named(key,"enter")){
    commands_->dispatchAction({UiAction::Kind::Activate,
      request->selectedIndex,{}});return;
  }
  if(named(key,"left")){
    commands_->dispatchAction({UiAction::Kind::Back,0,{}});return;
  }
  if(named(key,"up")||named(key,"down")){
    if(request->choices.empty())return;
    const auto index=named(key,"up")?
      (request->selectedIndex==0?request->choices.size()-1:
        request->selectedIndex-1):
      (request->selectedIndex+1)%request->choices.size();
    commands_->dispatchAction({UiAction::Kind::MoveSelection,index,
      named(key,"up")?"up":"down"});
    return;
  }
  if(key->sequence)commands_->dispatchAction(
    {UiAction::Kind::TextChange,0,*key->sequence});
}
void Shell::onSearchKey(const keyboard_input::KeyRef& key){
  if(named(key,"escape")||(named(key,"c")&&key->ctrl)||
     (named(key,"g")&&key->ctrl)){closeSearch(false);return;}
  searchEditor_->onParsedKey(*key,searchInput_);
  requestRedraw();
}
bool Shell::onCopyMouse(const ftxui::Mouse& mouse){
  if(!copyView_)return false;
  if(mouse.button!=ftxui::Mouse::Left)return true;
  if(mouse.motion==ftxui::Mouse::Pressed)copyView_->press(mouse.x,mouse.y);
  else if(mouse.motion==ftxui::Mouse::Moved)copyView_->move(mouse.x,mouse.y);
  else if(mouse.motion==ftxui::Mouse::Released){
    copyScrollTimer_.cancel();copyScrollTimer_={};
    if(auto text=copyView_->release(mouse.x,mouse.y))
      imports_.copyMessageText(*text);
  }
  if(copyView_->dragging())scheduleCopyScroll();
  requestRedraw();return true;
}
void Shell::scheduleCopyScroll(){
  copyScrollTimer_.cancel();copyScrollTimer_={};
  if(!copyView_||!copyView_->dragging()||!copyView_->edgeDirection())return;
  auto weak=weak_from_this();
  copyScrollTimer_=imports_.owner.setTimeout(80,[weak]{
    auto self=weak.lock();if(!self||!self->copyView_)return;
    self->copyScrollTimer_={};
    const auto before=self->copyView_->top();
    self->copyView_->scrollBy(self->copyView_->edgeDirection());
    if(self->copyView_->top()!=before){
      self->requestRedraw();self->scheduleCopyScroll();
    }
  });
}
void Shell::scroll(double rows){
  if(copyView_){copyView_->scrollBy(static_cast<int>(rows));requestRedraw();return;}
  if(driver_->mode()==terminal_ftxui::Mode::Fullscreen){
    viewport_->scrollBy(rows);
    if(transcript_)navigation_->disarmSearch();
  }else{
    const auto layout=terminal_ftxui::layoutPresentation(
      presentation(),presentation().viewport.columns,
      presentation().viewport.rows);
    scrollViewport(presentation(),static_cast<std::ptrdiff_t>(rows),
      layout.transcriptRowCount,layout.bodyHeight);
  }
  requestRedraw();
}
void Shell::jumpToBottom(){
  if(driver_->mode()==terminal_ftxui::Mode::Fullscreen)
    viewport_->scrollToBottom();
  else presentation().viewport.follow=true;
  if(!batConversation_)imports_.onRepin();
  actions_->setCursor({});
  newMessageCount_=0;requestRedraw();
}
void Shell::dispatchMessageAction(const std::string& action){
  actions_->dispatch(action);requestRedraw();
}
void Shell::setTranscript(bool active){
  if(!active){copyScrollTimer_.cancel();copyScrollTimer_={};copyView_.reset();}
  if(transcript_==active)return;
  if(batConversation_){
    transcript_=active;
    if(!active){
      transcriptExport_.reset();searchOpen_=false;searchQuery_.clear();searchDraft_.clear();
      searchInput_.text.clear();searchInput_.caretUtf16=0;searchCount_=searchCurrent_=0;
      navigation_->setSearchQuery("");navigation_->disarmSearch();
    }
    driver_->setMode(terminal_ftxui::Mode::Fullscreen);requestRedraw();return;
  }
  if(!runtime_)throw std::logic_error("Conversation runtime binding required");
  if(active){
    runtime_->handleEnterTranscript();
    transcript_=true;
    driver_->setMode(terminal_ftxui::Mode::Fullscreen);
    runtime_->setTranscriptScreen(true,true);
  }else{
    transcriptExport_.reset();
    runtime_->handleExitTranscript();
    retained_->unmountAll();
    surfaceState_.unmounted(imports_.setTerminalProgress);
    searchOpen_=false;searchQuery_.clear();searchDraft_.clear();
    searchInput_.text.clear();searchInput_.caretUtf16=0;
    searchCount_=searchCurrent_=0;
    navigation_->setSearchQuery("");navigation_->disarmSearch();
    transcript_=false;
    runtime_->setTranscriptScreen(false,false);
    driver_->setMode(baseMode_);
  }
  requestRedraw();
}
void Shell::openSearch(){
  if(!transcript_||driver_->mode()!=terminal_ftxui::Mode::Fullscreen)return;
  navigation_->setAnchor();searchOpen_=true;searchDraft_.clear();
  searchInput_.text.clear();searchInput_.caretUtf16=0;
  navigation_->setSearchQuery("");requestRedraw();
}
void Shell::setSearchQuery(std::string query){
  if(searchOpen_){
    searchDraft_=query;searchInput_.text=query;
  }
  else searchQuery_=query;
  navigation_->setSearchQuery(query);requestRedraw();
}
void Shell::closeSearch(bool commit){
  if(!searchOpen_)return;
  if(commit){
    searchQuery_=searchCount_>0?searchDraft_:std::string{};
    searchOpen_=false;
    if(searchQuery_.empty()){
      searchCount_=searchCurrent_=0;
      navigation_->setSearchQuery("");
    }
  }else{
    searchOpen_=false;
    navigation_->setSearchQuery("");
    navigation_->setSearchQuery(searchQuery_);
  }
  searchDraft_.clear();requestRedraw();
  searchInput_.text.clear();searchInput_.caretUtf16=0;
}
} // namespace batbox::interactive::conversation_shell
