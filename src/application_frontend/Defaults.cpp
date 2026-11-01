#include "application_frontend/Defaults.hpp"
#include "interactive/conversation_shell/Shell.hpp"
#include "interactive/context/GadgetPool.hpp"
#include "messages/TextContent.hpp"
#include "messages/Context.hpp"
#include "media/Media.hpp"
#include "compaction/Compaction.hpp"
#include <chrono>
#include <filesystem>
#include <stdexcept>

namespace batbox::application_frontend {
namespace {
template<class T> event_loop::Deferred<T> ready(event_loop::Loop& owner,T value){
  event_loop::Deferred<T> out(owner);out.resolve(std::move(value));return out;
}
template<class T> event_loop::Deferred<T> unsupported(event_loop::Loop& owner,const char* feature){
  event_loop::Deferred<T> out(owner);
  out.reject(std::make_exception_ptr(std::runtime_error(
    std::string(feature)+" is not bound in the native frontend")));
  return out;
}
std::string contentText(const Json& value){
  return messages::getContentText(value::property(value::property(value,"message"),"content")).value_or("");
}
bool selectable(const interactive::TurnMessageRef& message){
  if(!message||value::property(*message,"type")!="user"||
     value::truthy(value::property(*message,"isMeta"))||
     value::truthy(value::property(*message,"isCompactSummary"))||
     value::truthy(value::property(*message,"isVisibleInTranscriptOnly")))return false;
  const auto& content=value::property(value::property(*message,"message"),"content");
  if(content.is_array()&&!content.empty()&&value::property(content.front(),"type")=="tool_result")return false;
  const auto text=contentText(*message);
  for(const auto* tag:{"<local-command-stdout>","<local-command-stderr>",
      "<bash-stdout>","<bash-stderr>","<task-notification>","<tick>",
      "<teammate-message"})if(text.find(tag)!=std::string::npos)return false;
  return !value::trim(text).empty();
}
bool onlySynthetic(const interactive::TurnMessagesRef& messages,std::size_t index){
  for(std::size_t i=index+1;i<messages->size();++i){
    const auto& message=(*messages)[i];if(!message)continue;
    const auto type=value::property(*message,"type");
    if(type=="progress"||type=="system"||type=="attachment")continue;
    if(type=="user"){
      if(value::truthy(value::property(*message,"isMeta")))continue;
      const auto& content=value::property(value::property(*message,"message"),"content");
      if(content.is_array()&&!content.empty()&&value::property(content.front(),"type")=="tool_result")continue;
      const auto text=contentText(*message);
      if(text==messages::INTERRUPT_MESSAGE||text==messages::INTERRUPT_MESSAGE_FOR_GADGET_USE||
         text==messages::CANCEL_MESSAGE)continue;
      return false;
    }
    if(type=="assistant"){
      const auto& content=value::property(value::property(*message,"message"),"content");
      if(content.is_array())for(const auto& block:content){
        if(value::property(block,"type")=="tool_use")return false;
        if(value::property(block,"type")=="text"&&
           value::property(block,"text").is_string()&&
           !value::trim(block.at("text").get<std::string>()).empty())return false;
      }
      continue;
    }
    return false;
  }
  return true;
}
}
bool selectableUserMessage(const interactive::TurnMessageRef& message){
  return selectable(message);
}
bool messagesAfterAreOnlySynthetic(const interactive::TurnMessagesRef& messages,
    std::size_t index){return onlySynthetic(messages,index);}
std::function<event_loop::Deferred<user_input::InputResult>(
    const std::string&,const Json&,const Json&)>
makeIndigenousBashCommand(IndigenousBashCommandInputs inputs){
  if(!inputs.getBashGadget)
    throw std::invalid_argument("Bash input requires the live Bash tool");
  if(!inputs.showProgress)inputs.showProgress=[](const std::string&,
      const std::optional<Json>&){};
  if(!inputs.clearProgress)inputs.clearProgress=[]{};
  auto shared=std::make_shared<IndigenousBashCommandInputs>(std::move(inputs));
  return [shared](const std::string& command,const Json& preceding,
      const Json& attachments){
    event_loop::Deferred<user_input::InputResult> output(shared->owner);
    try{
      if(!shared->owner.isOwnerThread())
        throw std::logic_error("Bash input must start on the owner loop");
      auto bash=std::make_shared<gadgets::Gadget>(shared->getBashGadget());
      if(bash->name!="Bash"||!bash->call)
        throw std::invalid_argument("Live Bash tool is unavailable");
      user_input::IndigenousBashInputConfig config{
        *bash,nullptr,
        shared->getContext?shared->getContext():gadgets::GadgetCallContext{},
        shared->storage,Json::object(),
        [](){return false;},[](){return std::string("bash");},
        shared->showProgress,shared->clearProgress};
      auto services=std::make_shared<user_input::BashInputServices>(
        user_input::bindIndigenousBashInput(shared->owner,shared->messages,
          std::move(config)));
      user_input::processBashCommand(command,preceding,attachments,*services)
        .then([output,bash,services](const user_input::InputResult* value,
            const std::exception_ptr& error){
          if(error)output.reject(error);
          else output.resolve(*value);
        });
    }catch(...){output.reject(std::current_exception());}
    return output;
  };
}
Bundle::Bundle(application_runtime::Inputs& runtime,Inputs inputs):
  owner_(runtime.owner),config_(runtime.config),state_(runtime.presentation),
  store_(runtime.store),inputs_(std::move(inputs)),commands_(runtime.commands){
  if(!inputs_.messages||!inputs_.poetry||!inputs_.flight||!inputs_.permission||!store_)
    throw std::invalid_argument("Frontend requires message, prompt, session, permission and state owners");
  if(!inputs_.feature)inputs_.feature=[](const std::string&){return false;};
  if(!inputs_.log)inputs_.log=[](const std::string&){};
  if(!inputs_.onError)inputs_.onError=[](std::exception_ptr error){if(error)std::rethrow_exception(error);};
  if(!inputs_.redraw)inputs_.redraw=[]{};
}
std::shared_ptr<Bundle> Bundle::bind(application_runtime::Inputs& runtime,Inputs inputs){
  auto self=std::shared_ptr<Bundle>(new Bundle(runtime,std::move(inputs)));
  self->install(runtime);return self;
}
void Bundle::rebindEnvironment(application::Config config,
    std::shared_ptr<application_poetry::Provider> poetry,
    std::vector<user_input::CommandEntry> commands,Json initialMcpClients,
    std::shared_ptr<application_file_history::History> fileHistory){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Frontend rebind requires owner loop");
  if(!poetry)throw std::invalid_argument("Frontend rebind needs prompt provider");
  config_=std::move(config);
  inputs_.poetry=std::move(poetry);
  if(fileHistory)inputs_.fileHistory=std::move(fileHistory);
  commands_=std::move(commands);
  if(initialMcpClients.is_array())
    inputs_.initialMcpClients=std::move(initialMcpClients);
}
std::shared_ptr<user_input::UserInputServices> Bundle::makeUserInputServices(){
  struct Owned {
    user_input::SlashServices slash;
    user_input::UserInputServices input;
    Owned(event_loop::Loop& owner,messages::Services& messages,
        const std::vector<user_input::CommandEntry>& commands):
      slash{owner,messages,commands,{},{},{},{},{},{},{},{}},
      input{owner,messages,slash,{},{},{},{},{},{},{},{},{},{},{},{},{}}{}
  };
  auto owned=std::make_shared<Owned>(owner_,*inputs_.messages,commands_);
  auto& slash=owned->slash;
  slash.pathExists=[this](const std::string& path){
    std::error_code error;return ready(owner_,std::filesystem::exists(path,error));};
  slash.recordSkillUsage=[](const std::string&){};
  slash.buildPostCompactMessages=compaction::buildPostCompactMessages;
  slash.resetMicrocompactState=[]{};
  slash.nowISOAfterMs=[this](double){return inputs_.messages->nowISO();};
  slash.logError=inputs_.log;
  slash.isAbortError=[](const std::exception_ptr&){return false;};
  slash.isFullscreen=[this]{return config_.fullscreen;};
  auto& input=owned->input;
  input.getAppState=[store=store_]{return store->getState()->snapshot();};
  input.queryCheckpoint=inputs_.log;
  input.storeImages=[this](const Json& pasted){
    if(pasted.is_object()&&pasted.empty())return ready(owner_,Json::object());
    if(inputs_.storeImages)return inputs_.storeImages(pasted);
    if(inputs_.media)return inputs_.media->storeImages(pasted);
    return unsupported<Json>(owner_,"Pasted images");
  };
  input.resizeImageBlock=[this](const Json& block){
    if(inputs_.resizeImage)return inputs_.resizeImage(block);
    if(inputs_.media)return inputs_.media->resizeImageBlock(block);
    return unsupported<Json>(owner_,"Image resizing");
  };
  input.createImageMetadataText=[this](const Json& dimensions,
      const std::optional<std::string>& path)->std::optional<std::string>{
    if(inputs_.media)return inputs_.media->imageMetadata(dimensions,path);
    return media::createImageMetadataText(dimensions,path);
  };
  input.getAttachmentMessages=[this](const std::string& poetry,const Json& selection,
      const Json& images,const Json& messages,const std::optional<std::string>& source){
    if(inputs_.attachments)return inputs_.attachments(poetry,selection,images,messages,source);
    return ready(owner_,Json::array());
  };
  input.processBashCommand=[this](const std::string& command,const Json& preceding,const Json& attachments){
    if(inputs_.bashCommand)return inputs_.bashCommand(command,preceding,attachments);
    return unsupported<user_input::InputResult>(owner_,"Bash input mode");
  };
  input.isBridgeSafeCommand=[](const Json&){return false;};
  input.feature=inputs_.feature;
  input.hasUltraplanKeyword=[](const std::string&){return false;};
  input.replaceUltraplanKeyword=[](const std::string& text){return text;};
  input.executeUserPoetrySubmitHooks=[this](const std::string& poetry,
      const std::string& mode,std::stop_token signal){
    if(inputs_.userPoetryHooks)return inputs_.userPoetryHooks(poetry,mode,signal);
    if(inputs_.feature("HOOK_PROMPTS"))
      throw std::runtime_error("UserPromptSubmit hooks are enabled without native hook services");
    return instructions::hooks::runtime::HookEventGenerator(owner_,
      []()->std::optional<instructions::hooks::runtime::ExecuteHooksInput>{return {};},
      [](instructions::hooks::runtime::ExecuteHooksInput)
        ->instructions::hooks::runtime::HookYieldCursor{return {};});
  };
  auto* inputPointer=&owned->input;
  return std::shared_ptr<user_input::UserInputServices>(std::move(owned),inputPointer);
}
interactive::submit::PoetrySubmitRequest Bundle::submitRequest(const std::string& text,
    const interactive::InputState& input)const{
  interactive::submit::PoetrySubmitRequest request;
  request.mode=text.starts_with('!')?"bash":"prompt";
  request.input=request.mode=="bash"?text.substr(1):text;
  const auto configured=value::property(store_->getState()->snapshot(),"mainLoopModel");
  request.mainLoopModel=configured.is_string()&&!configured.get<std::string>().empty()
    ?configured.get<std::string>():config_.model;
  request.querySource="repl_main_thread";request.streamMode=state_.streamMode;
  request.pastedContents=input.pastedContents;
  request.isExternalLoading=state_.loading;
  if(inputs_.activeAbort)request.activeAbort=inputs_.activeAbort();
  if(inputs_.hasInterruptibleGadgetInProgress)
    request.hasInterruptibleGadgetInProgress=inputs_.hasInterruptibleGadgetInProgress();
  request.messages=Json::array();for(const auto& message:state_.messages)
    if(message)request.messages.push_back(*message);
  request.helpers.onInputChange=[this](const std::string& value){
    state_.input.text=value;++state_.revision;inputs_.redraw();};
  request.helpers.setCursorOffset=[this](std::size_t value){
    state_.input.caretUtf16=value;++state_.revision;inputs_.redraw();};
  request.helpers.clearBuffer=[this]{state_.input.text.clear();state_.input.caretUtf16=0;
    ++state_.revision;inputs_.redraw();};
  request.helpers.resetHistory=[this]{
    if(inputs_.getShell)if(auto shell=inputs_.getShell())shell->resetHistory();};
  request.helpers.setPastedContents=[this](const Json& value){
    state_.input.pastedContents=value;++state_.revision;inputs_.redraw();};
  return request;
}
interactive::QueryExecutionParams Bundle::queryParams()const{
  interactive::QueryExecutionParams params;
  params.titleDisabled=true;
  params.initialMcpClients=inputs_.initialMcpClients;
  const auto snapshot=store_->getState()->snapshot();
  params.gadgetPermissionContext=value::property(snapshot,"toolPermissionContext");
  if(!params.gadgetPermissionContext.is_object())params.gadgetPermissionContext=Json::object();
  return params;
}
void Bundle::install(application_runtime::Inputs& runtime){
  auto self=shared_from_this();
  runtime.feature=inputs_.feature;
  runtime.nowMs=[]{
    return std::chrono::duration<double,std::milli>(
      std::chrono::steady_clock::now().time_since_epoch()).count();};
  runtime.onDetachedError=inputs_.onError;
  runtime.onQueryContextCreated=inputs_.onQueryContextCreated;
  runtime.requestRedraw=inputs_.redraw;
  runtime.makeQueryParams=[self]{return self->queryParams();};
  runtime.makeSubmitRequest=[self](const std::string& text,const interactive::InputState& input){
    ++self->submitCount_;return self->submitRequest(text,input);};
  runtime.loading.getTipToShowOnSpinner=[self](const interactive::lifecycle::TipContext&){
    return ready(self->owner_,interactive::lifecycle::TipRef{});};
  runtime.loading.recordShownTip=[](const interactive::lifecycle::TipRef&){};
  runtime.loading.endInteractionSpan=[]{};
  runtime.loading.clearSpeculativeChecks=[]{};
  runtime.transcript.setHighlight=[](const std::string&){};
  runtime.transcript.clearPositions=[]{};
  runtime.transcript.cancelEditorTimer=[]{};
  runtime.transcript.requestRedraw=inputs_.redraw;
  runtime.idle.nowMs=runtime.nowMs;
  runtime.idle.getLastInteractionTime=[self]{return self->lastInteractionMs_;};
  runtime.idle.messageIdleThresholdMs=[]{return 3600000.0;};
  runtime.idle.focusedDialogPresent=[self]{return self->state_.modal.has_value();};
  runtime.idle.sendIdlePoetry=[](const std::string&,const std::string&){};
  runtime.idle.idleReturnMode=[]{return std::string("off");};
  runtime.turn.isBatSwarmsEnabled=[self]{return self->inputs_.feature("AGENT_SWARMS");};
  runtime.turn.hasRunningSwarmBats=[]{return false;};
  runtime.turn.getTeamName=[]()->std::optional<std::string>{return {};};
  runtime.turn.getBatName=[]()->std::optional<std::string>{return {};};
  runtime.turn.setMemberActive=[](const std::string&,const std::string&,bool){};
  runtime.turn.sendBridgeResult=[]{};
  runtime.turn.removeLastFromHistory=[self]{
    if(self->inputs_.history)self->inputs_.history->removeLast();};
  runtime.turn.mrOnBeforeQuery=[self](const std::string& input,
      const interactive::TurnMessagesRef& messages,std::size_t count){
    if(self->inputs_.beforeQuery)return self->inputs_.beforeQuery(input,messages,count);
    return ready(self->owner_,event_loop::Unit{});
  };
  runtime.turn.mrOnTurnComplete=[self](const interactive::TurnMessagesRef& messages,bool){
    if(!self->inputs_.persistOnTurnComplete)
      return ready(self->owner_,event_loop::Unit{});
    event_loop::Deferred<event_loop::Unit> output(self->owner_);
    auto pending=self->inputs_.flight->record(*messages);
    pending.then([output](const std::optional<std::string>*,const std::exception_ptr& error){
      if(error)output.reject(error);else output.resolve({});});
    return output;
  };
  runtime.turn.isLoggableMessage=[](const interactive::TurnMessageRef& message){
    return message&&value::property(*message,"type")!="progress"&&
      value::property(*message,"type")!="attachment";};
  runtime.turn.selectableUserMessagesFilter=selectable;
  runtime.turn.createTurnDurationMessage=[messages=inputs_.messages](double duration,
      const std::optional<Json>& budget,std::size_t count){
    return std::make_shared<Json>(messages::createTurnDurationMessage(duration,budget,count,*messages));};
  runtime.turn.isViewingBatMission=[](){return false;};
  runtime.turn.messagesAfterAreOnlySynthetic=onlySynthetic;
  runtime.turn.restoreMessageSync=[self](const interactive::TurnMessageRef& message){
    // The same restore path is used by both transcript editing and
    // interrupted turns: it rewinds the conversation, restores mode and text,
    // and reconstructs image paste blocks with their original reference IDs.
    self->initializeRewind();
    self->rewind_->restoreMessageSync(message);
  };
  auto& execution=runtime.execution;
  execution.mergeClients=interactive::context::mergeClients;
  execution.diagnosticQueryStart=[self](const Json& clients){
    if(self->inputs_.diagnosticQueryStart)self->inputs_.diagnosticQueryStart(clients);};
  execution.getConnectedIdeClient=[](const Json&)->Json{return Json();};
  execution.closeOpenDiffs=[](const Json&){};
  execution.markOnboardingComplete=[]{};
  execution.resetTurnHookDuration=[]{};
  execution.resetTurnGadgetDuration=[]{};
  execution.resetTurnClassifierDuration=[]{};
  execution.logQueryProfileReport=[]{};
  execution.generateFlightTitle=[self](const std::string&,
      const std::shared_ptr<interactive::TurnAbort>&){return ready(self->owner_,Json());};
  execution.setFlightTitle=[self](const std::string& title){
    if(self->inputs_.setFlightTitle)self->inputs_.setFlightTitle(title);};
  execution.checkpoint=inputs_.log;
  execution.isCompactBoundary=[](const interactive::TurnMessageRef& message){
    return message&&messages::isCompactBoundaryMessage(*message);};
  execution.uuid=[messages=inputs_.messages]{return messages->randomUUID();};
  execution.getScratchpadDir=[self]{return self->config_.stateDirectory.string();};
  execution.getQuerySource=[]{return std::string("repl_main_thread");};
  execution.checkBypassPermissions=[permission=inputs_.permission](const Json& context,
      const std::shared_ptr<app_state::AppStateStore>& store){
    return permission->checkBypassPermissions(context,store);};
  execution.checkAutoMode=[self](const Json&,
      const std::shared_ptr<app_state::AppStateStore>&,const Json&){
    return unsupported<Json>(self->owner_,"Transcript classifier auto mode");};
  execution.getSystemPoetry=[self](const execution::Context& context,
      const std::string& model,const std::vector<std::string>&,const Json& clients){
    Json gadgets=Json::array();for(const auto* gadget:context.gadgets)if(gadget)
      gadgets.push_back(Json{{"name",gadget->name}});
    auto mcp=clients.is_array()?clients:Json::array();
    event_loop::Deferred<Json> output(self->owner_);
    auto pending=self->inputs_.poetry->systemPoetry(gadgets,model,mcp);
    pending.then([output](const std::vector<std::string>* result,
        const std::exception_ptr& error){
      if(error)output.reject(error);else if(result)output.resolve(Json(*result));
      else output.reject(std::make_exception_ptr(std::logic_error("System prompt missing")));
    });return output;
  };
  execution.getUserContext=[self]{return self->inputs_.poetry->userContext();};
  execution.getSystemContext=[self]{return self->inputs_.poetry->systemContext();};
  execution.getCoordinatorUserContext=[](const Json&,
      const std::optional<std::string>&){return Json::object();};
  execution.scratchpadEnabled=[]{return false;};
  execution.isProactiveActive=[]{return false;};
  execution.terminalFocused=[self]{return self->state_.input.focused;};
  execution.effectivePoetry.feature=inputs_.feature;
  execution.effectivePoetry.coordinatorModeEnabled=[]{return false;};
  execution.effectivePoetry.isProactiveActive=[]{return false;};
  execution.effectivePoetry.isBuiltInBat=[](const Json&){return false;};
  execution.effectivePoetry.getCoordinatorSystemPoetry=[](){
    throw std::runtime_error("Coordinator prompt is not bound in native frontend");
    return std::string{};};
  execution.launchMonsterObserver=[self](const interactive::TurnMessagesRef& messages){
    if(self->inputs_.launchMonsterObserver)self->inputs_.launchMonsterObserver(messages);
    else throw std::runtime_error("Monster observer is enabled without native observer binding");};
  execution.onTurnComplete=[self](const interactive::TurnMessagesRef&){
    return ready(self->owner_,event_loop::Unit{});};
  runtime.bindSubmitProviders=[self](interactive::submit::SubmitServices& submit){
    submit.makeUserInputServices=[self](const std::shared_ptr<interactive::TurnAbort>&,
        const std::string&){return self->makeUserInputServices();};
    submit.onBeforeQuery=[self](const std::string&,
        const interactive::TurnMessagesRef&){return ready(self->owner_,true);};
    submit.invokeImmediateUi=[self](const user_input::CommandEntry& entry,
        const std::string& args,interactive::submit::ImmediateContext,
        std::function<void(const interactive::CommandCompletion&)> done){
      event_loop::Deferred<event_loop::Unit> output(self->owner_);
      if(!entry.invokeUi)return unsupported<event_loop::Unit>(self->owner_,"Immediate UI command");
      auto pending=entry.invokeUi(args);
      pending.then([output,done=std::move(done)](const user_input::UiCompletion* value,
          const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        interactive::CommandCompletion result;
        result.result=value->result;result.options.shouldQuery=value->shouldQuery;
        result.options.metaMessages=value->metaMessages;
        result.options.nextInput=value->nextInput;
        result.options.submitNextInput=value->submitNextInput;
        result.options.display=value->display=="skip"?
          interactive::DoneOptions::Display::Skip:
          value->display=="system"?interactive::DoneOptions::Display::System:
          interactive::DoneOptions::Display::Default;
        done(result);output.resolve({});
      });return output;
    };
    submit.isCommandEnabled=[](const Json& command){
      return value::property(command,"enabled")!=false;};
    submit.gracefulShutdownSync=[self](int){
      if(self->inputs_.shutdown)self->inputs_.shutdown();};
    submit.notifyImmediate=[self](const std::string&,const std::string& value){
      if(self->inputs_.showImmediateNotice)self->inputs_.showImmediateNotice(value);
      if(self->inputs_.log)self->inputs_.log(value);
    };
    submit.logForDebugging=self->inputs_.log;submit.queryCheckpoint=self->inputs_.log;
    submit.startQueryProfile=[]{ };
    submit.resetHistory=[self]{
      if(self->inputs_.getShell)if(auto shell=self->inputs_.getShell())
        shell->resetHistory();};
    submit.fileHistoryEnabled=[self]{
      return self->inputs_.fileHistory&&self->inputs_.fileHistory->enabled();};
    submit.selectableUserMessage=selectable;
    submit.fileHistoryMakeSnapshot=[self](const Json& id){
      const auto& history=self->inputs_.fileHistory;
      if(!history||!history->enabled()||!id.is_string())return;
      auto pending=history->makeSnapshot(id.get<std::string>());
      pending.then([self](const event_loop::Unit*,const std::exception_ptr& error){
        if(error&&self->inputs_.onError)self->inputs_.onError(error);
      });
    };
    submit.resolveSkillModelOverride=[](const std::string& model,const std::string&){return model;};
  };
  runtime.bindRuntimeProviders=[self](interactive::repl_runtime::RuntimeServices& service){
    service.reverifyApiKey=[self]{return ready(self->owner_,event_loop::Unit{});};
    service.shutdownDiagnostics=[self]{return ready(self->owner_,event_loop::Unit{});};
    service.getMemoryFiles=[self]{
      event_loop::Deferred<std::vector<interactive::repl_runtime::MemoryFile>> output(self->owner_);
      auto pending=self->inputs_.poetry->memoryFiles();
      pending.then([output](const context::MemoryFiles* files,const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        if(!files){output.reject(std::make_exception_ptr(
          std::logic_error("Memory file discovery returned no result")));return;}
        std::vector<interactive::repl_runtime::MemoryFile> mapped;
        mapped.reserve(files->size());
        for(const auto& file:*files)mapped.push_back({file.type,file.path,file.content,
          file.parent,file.rawContent,file.contentDiffersFromDisk});
        output.resolve(std::move(mapped));
      });
      return output;
    };
    service.logForDebugging=self->inputs_.log;
    service.makeQueuedRequest=[self](const interactive::submit::CommandList&){
      return self->submitRequest("",self->state_.input);};
    service.hasActiveLocalUi=[self]{return self->state_.modal.has_value();};
    service.createUserMessage=[messages=self->inputs_.messages](const std::string& text,
        std::optional<bool> meta){
      Json args={{"content",text}};if(meta)args["isMeta"]=*meta;
      return std::make_shared<Json>(messages::createUserMessage(args,*messages));};
    service.isRemoteSafeCommand=[](const interactive::repl_runtime::LocalCommandRef&){return false;};
    service.publishLocalCommands=[](const interactive::repl_runtime::LocalCommands&){};
    service.recordUserActivity=[self]{
      self->lastInteractionMs_=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now().time_since_epoch()).count();};
    service.startBackgroundHousekeeping=[]{};
    service.getSubmitCount=[self]{return self->submitCount_;};
    service.updateLastInteractionTime=[self](bool){
      self->lastInteractionMs_=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now().time_since_epoch()).count();};
  };
}
} // namespace batbox::application_frontend
