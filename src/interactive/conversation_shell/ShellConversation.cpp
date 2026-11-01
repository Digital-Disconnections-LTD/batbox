#include "interactive/conversation_shell/Shell.hpp"
#include "interactive/TranscriptRows.hpp"
#include "history/References.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <stdexcept>
#include <unordered_map>

namespace batbox::interactive::conversation_shell {
namespace {
std::string text(const Json& value,const char* name){
  const auto& item=value::property(value,name);
  return item.is_string()?item.get<std::string>():std::string{};
}
bool settled(const std::string& status){
  return status=="completed"||status=="failed"||status=="killed";
}
}
PresentationState& Shell::presentation(){
  return batConversation_?batConversation_->presentation:imports_.presentation;
}
const PresentationState& Shell::presentation()const{
  return batConversation_?batConversation_->presentation:imports_.presentation;
}
std::optional<std::string> Shell::activeBatId()const{
  return batConversation_?std::optional(batConversation_->id):std::nullopt;
}
void Shell::resetConversationNavigation(){
  transcriptExport_.reset();
  searchOpen_=false;searchQuery_.clear();searchDraft_.clear();
  searchInput_.text.clear();searchInput_.caretUtf16=0;
  searchCount_=searchCurrent_=0;positions_.reset();sticky_={};
  cursor_.reset();actions_->setCursor({});navigation_->setSearchQuery("");
  prepared_.reset();sliceAnchor_.reset();previousThinking_.reset();thinkingRef_.reset();
  suggestions_.clear();++completionGeneration_;
  retained_->unmountAll();surfaceState_={};
  pendingImageSpace_=false;
  presentation().input.focused=true;
  viewport_->scrollToBottom();
}
void Shell::openBatConversation(const std::string& id){
  if(!imports_.owner.isOwnerThread())throw std::logic_error("Bat view requires owner loop");
  if(!imports_.observeBatMission||!imports_.batMissionSnapshot)
    throw std::runtime_error("Bat conversations are unavailable");
  const auto generation=++batOpenGeneration_;
  auto weak=weak_from_this();
  auto pending=imports_.observeBatMission(id);
  pending.then([weak,id,generation](const Json* mission,const std::exception_ptr& error){
    auto self=weak.lock();if(!self)return;
    if(error){
      if((!self->batConversation_||self->batConversation_->id!=id)&&self->imports_.releaseBatMission)
        self->imports_.releaseBatMission(id);
      try{std::rethrow_exception(error);}
      catch(const std::exception& issue){self->transcriptStatus(issue.what());}
      catch(...){self->transcriptStatus("Could not open this Bat conversation");}
      return;
    }
    if(!mission||!mission->is_object())return;
    if(generation!=self->batOpenGeneration_||!self->started_){
      if(self->imports_.releaseBatMission)self->imports_.releaseBatMission(id);
      return;
    }
    if(!self->batConversation_){
      if(self->transcript_)self->setTranscript(false);
      self->leaderScrollTop_=self->viewport_->viewportTop();
      self->leaderAtBottom_=self->viewport_->atBottom();
      if(self->historyNavigator_)self->historyNavigator_->reset();
    }else{
      self->batDrafts_[self->batConversation_->id]=self->batConversation_->presentation.input;
      if(self->batConversation_->id!=id&&self->imports_.releaseBatMission)
        self->imports_.releaseBatMission(self->batConversation_->id);
    }
    BatConversation conversation;conversation.id=id;
    conversation.presentation.viewport=self->imports_.presentation.viewport;
    conversation.presentation.viewport.follow=true;
    conversation.presentation.conversationId="agent:"+id;
    if(const auto draft=self->batDrafts_.find(id);draft!=self->batDrafts_.end())
      conversation.presentation.input=draft->second;
    self->batConversation_=std::move(conversation);
    self->updateBatConversation(*mission);
    self->resetConversationNavigation();self->transcript_=false;
    self->driver_->setMode(terminal_ftxui::Mode::Fullscreen);
    self->requestRedraw();
  });
}
void Shell::returnToLeader(){
  ++batOpenGeneration_;
  if(!batConversation_)return;
  batDrafts_[batConversation_->id]=batConversation_->presentation.input;
  if(imports_.releaseBatMission)imports_.releaseBatMission(batConversation_->id);
  batConversation_.reset();
  resetConversationNavigation();transcript_=false;
  driver_->setMode(baseMode_);
  if(!leaderAtBottom_&&leaderScrollTop_)viewport_->scrollTo(*leaderScrollTop_);
  leaderScrollTop_.reset();syncFocus();requestRedraw();
}
void Shell::refreshBatConversation(){
  if(!batConversation_)return;
  // Command/gadget modal state belongs to the live leader owner. Displaying it
  // never swaps, appends to, or persists the child's message array as the leader.
  batConversation_->presentation.modal=imports_.presentation.modal;
  if(const auto mission=imports_.batMissionSnapshot(batConversation_->id))
    updateBatConversation(*mission);
  else returnToLeader();
}
void Shell::updateBatConversation(const Json& mission){
  if(!batConversation_)return;
  auto& conversation=*batConversation_;auto& state=conversation.presentation;
  conversation.label=text(mission,"description");
  if(conversation.label.empty())conversation.label=text(mission,"agentType");
  if(conversation.label.empty())conversation.label=conversation.id;
  conversation.status=text(mission,"status");
  const bool running=conversation.status=="running"||conversation.status=="pending";
  state.loading=running||conversation.submitting;
  const auto& raw=value::property(mission,"messages");
  const bool messagesChanged=raw.is_array()&&raw!=conversation.messages;
  const auto rawCount=raw.is_array()?raw.size():conversation.messages.size();
  // Follow-ups sent to a running Bat wait in pendingMessages until its next
  // Gadget round drains them; show that wait and its delivery in the view.
  const auto& pending=value::property(mission,"pendingMessages");
  const auto waiting=pending.is_array()?pending.size():std::size_t{0};
  if(waiting>0){conversation.queued=true;conversation.delivered=false;}
  else if(conversation.queued){
    conversation.queued=false;conversation.delivered=running;
    conversation.deliveredAfter=rawCount;
  }
  if(conversation.delivered&&(!running||rawCount>conversation.deliveredAfter+1))
    conversation.delivered=false;
  std::string statusLine;
  const auto resumeError=text(mission,"inputResumeError");
  if(conversation.submitting&&!running)statusLine="Resuming this Bat…";
  else if(waiting>0)statusLine=(waiting==1?std::string("1 message"):
    std::to_string(waiting)+" messages")+" queued · delivered at the next Gadget round";
  else if(conversation.delivered)statusLine="Queued message delivered · the Bat reads it this round";
  else if(settled(conversation.status)&&!resumeError.empty())
    statusLine="Could not resume this Bat: "+resumeError;
  if(messagesChanged||statusLine!=conversation.statusLine){
    std::unordered_map<std::string,MessageRef> old;
    for(const auto& message:state.messages)if(message){
      const auto id=text(*message,"uuid");if(!id.empty())old.emplace(id,message);
    }
    const Json& source=raw.is_array()?raw:conversation.messages;
    std::vector<MessageRef> messages;messages.reserve(source.size()+1);
    for(const auto& message:source){
      const auto id=text(message,"uuid");const auto found=old.find(id);
      if(found!=old.end()&&*found->second==message)messages.push_back(found->second);
      else messages.push_back(std::make_shared<Json>(message));
    }
    if(!statusLine.empty())messages.push_back(std::make_shared<Json>(Json{
      {"type","system"},{"subtype","informational"},{"level","notice"},
      {"isMeta",false},{"content",statusLine},
      {"uuid","bat-status:"+conversation.id},
      {"timestamp",source.empty()?Json(""):value::property(source.back(),"timestamp")}}));
    state.messages=std::move(messages);
    if(raw.is_array())conversation.messages=raw;
    conversation.statusLine=std::move(statusLine);
    rebuildTranscriptRows(state);++state.revision;
  }
  const auto& streaming=value::property(mission,"streamingText");
  state.streamingText=streaming.is_string()?std::optional(streaming.get<std::string>()):std::nullopt;
  const auto mode=text(mission,"streamMode");
  state.streamMode=mode.empty()?std::nullopt:std::optional(mode);
  const auto& thinking=value::property(mission,"streamingThinking");
  if(thinking.is_object()){
    StreamingThinking value{text(thinking,"thinking"),
      value::property(thinking,"isStreaming")==true,{}};
    const auto& ended=value::property(thinking,"streamingEndedAt");
    if(ended.is_number())value.streamingEndedAt=ended.get<double>();
    state.streamingThinking=std::move(value);
  }else state.streamingThinking.reset();
  state.streamingGadgetUses.clear();
  const auto& gadgets=value::property(mission,"streamingToolUses");
  if(gadgets.is_array())for(const auto& gadget:gadgets)state.streamingGadgetUses.push_back({
    value::property(gadget,"index"),value::property(gadget,"contentBlock"),text(gadget,"unparsedToolInput")});
}
bool Shell::submitBatPoetry(const std::string& value){
  if(!batConversation_)return false;
  const auto trimmed=value::trim(value);
  if(trimmed=="/leader"||trimmed=="/back"){
    presentation().input.text.clear();presentation().input.caretUtf16=0;
    returnToLeader();return true;
  }
  // Slash controls are application actions, whereas ordinary text targets the
  // selected child. Destructive conversation actions require returning first.
  if(trimmed.starts_with('/')){
    const auto end=trimmed.find_first_of(" \t\n");
    const auto command=trimmed.substr(0,end);
    for(const auto* local:{"/bats","/agents","/memory","/missions","/tasks","/bashes","/help","/exit","/quit"})
      if(command==local){
        const auto name=command=="/quit"?std::string("exit"):command.substr(1);
        const auto args=end==std::string::npos?std::string{}:value::trim(trimmed.substr(end));
        if(imports_.openControlCommand&&imports_.openControlCommand(name,args)){
          presentation().input.text.clear();presentation().input.caretUtf16=0;
          suggestions_.clear();++completionGeneration_;requestRedraw();
        }else transcriptStatus("Could not open "+command+" right now; close any open dialog and try again");
        return true;
      }
    transcriptStatus(command+" runs in the leader conversation; use /leader first "
      "(here: /bats, /missions, /memory, /help)");
    return true;
  }
  if(trimmed.empty())return true;
  if(!imports_.submitBatMessage){transcriptStatus("Bat follow-ups are unavailable");return true;}
  auto& conversation=*batConversation_;
  if(conversation.submitting)return true;
  auto& input=conversation.presentation.input;
  for(auto it=input.pastedContents.begin();it!=input.pastedContents.end();++it)
    if(value::property(it.value(),"type")=="image"){
      transcriptStatus("Send image attachments from the leader conversation");return true;
    }
  // Expand collapsed text pastes before handing the poetry to the child API.
  const std::string poetry=history::expandPastedTextRefs(value,input.pastedContents);
  conversation.submitting=true;
  const auto id=conversation.id;const auto draft=input;
  input.text.clear();input.caretUtf16=0;input.pastedContents=Json::object();
  suggestions_.clear();++completionGeneration_;requestRedraw();
  auto weak=weak_from_this();
  try{
    imports_.submitBatMessage(id,poetry).then([weak,id,draft](const event_loop::Unit*,
        const std::exception_ptr& error){
      auto self=weak.lock();if(!self)return;
      if(self->batConversation_&&self->batConversation_->id==id){
        self->batConversation_->submitting=false;
        if(error&&self->presentation().input.text.empty())self->presentation().input=draft;
        self->refreshBatConversation();self->requestRedraw();
      }
      if(error)self->reportDetached(error);
    });
  }catch(...){conversation.submitting=false;input=draft;reportDetached(std::current_exception());}
  return true;
}
} // namespace batbox::interactive::conversation_shell
