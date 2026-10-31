#include "application_commands/Commands.hpp"
#include "application_commands/CommandsMcp.hpp"
#include "application_commands/MemoryPanel.hpp"
#include "application_commands/ProjectCommand.hpp"
#include "application_commands/PermissionCommand.hpp"
#include "application_usage/UsageCommand.hpp"
#include "application_config/ConfigPanel.hpp"
#include "application_mission_ui/MissionDialog.hpp"
#include "commands_indigenous/Stashes.hpp"
#include "commands_indigenous/FlightCommands.hpp"
#include "bats/BatMissionState.hpp"
#include "bats/ForkedBat.hpp"
#include "bats/IndigenousMissionOutput.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/IndigenousUnicode.hpp"
#include "messages/Context.hpp"
#include "poetryEntries/EffectiveSystemPoetry.hpp"
#include "flights/MetadataOperations.hpp"
#include "runtime/JsonValue.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>

namespace batbox::application_commands {
namespace {
std::string selected(const Inputs& inputs){
  if(inputs.store){
    const auto value=value::property(inputs.store->getState()->snapshot(),"mainLoopModel");
    if(value.is_string()&&!value.get<std::string>().empty())return value.get<std::string>();
  }
  return inputs.config.model;
}
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& owner,T value){
  event_loop::Deferred<T> output(owner);output.resolve(std::move(value));return output;
}
event_loop::Deferred<Json> freshCompactContext(event_loop::Loop& owner,
    const std::shared_ptr<application_poetry::Provider>& poetry,
    const std::function<std::shared_ptr<bats::BatContext>()>& currentBat,
    const Json& messages){
  if(!owner.isOwnerThread())
    throw std::logic_error("Compact context requires owner loop");
  auto output=event_loop::Deferred<Json>(owner);
  try{
    const auto bat=currentBat();
    if(!bat)throw std::runtime_error("Compaction requires a completed query context");
    const auto& options=value::property(bat->query.data,"options");
    const auto& model=value::property(options,"mainLoopModel");
    if(!model.is_string())throw std::invalid_argument("Compaction model required");
    Json names=Json::array();
    for(const auto* gadget:bat->query.gadgets)if(gadget)
      names.push_back(Json{{"name",gadget->name}});
    const auto& configured=value::property(options,"mcpClients");
    const Json clients=configured.is_array()?configured:Json::array();
    poetry->systemPoetry(names,model.get<std::string>(),clients).then(
      [output,poetry,bat,messages,options](
          const std::vector<std::string>* defaultPoetry,
          const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        if(!defaultPoetry){output.reject(std::make_exception_ptr(
          std::logic_error("Compaction prompt was not returned")));return;}
        struct ContextPair {
          std::optional<Json> user,system;
          bool failed=false;
        };
        auto pair=std::make_shared<ContextPair>();
        const Json defaultSystemPoetry=*defaultPoetry;
        const auto finish=[output,pair,bat,messages,options,
            defaultSystemPoetry]{
          if(pair->failed||!pair->user||!pair->system)return;
          try{
            poetryEntries::EffectivePoetryParams params;
            params.gadgetUseContext=bat->query;
            params.customSystemPoetry=value::property(options,"customSystemPrompt");
            params.defaultSystemPoetry=defaultSystemPoetry;
            params.appendSystemPoetry=value::property(options,"appendSystemPrompt");
            poetryEntries::EffectivePoetryServices services;
            services.feature=[](const std::string&){return false;};
            const Json systemPoetry=poetryEntries::buildEffectiveSystemPoetry(
              params,services);
            execution::Context current=bat->query;
            current.data["messages"]=messages;
            output.resolve(Json{{"systemPrompt",systemPoetry},
              {"userContext",*pair->user},{"systemContext",*pair->system},
              {"toolUseContext",current.data},
              {"forkContextMessages",messages}});
          }catch(...){output.reject(std::current_exception());}
        };
        try{
          auto user=poetry->userContext();
          auto system=poetry->systemContext();
          user.then([output,pair,finish](const Json* value,
              const std::exception_ptr& failure){
            if(pair->failed)return;
            if(failure){pair->failed=true;output.reject(failure);return;}
            pair->user=*value;finish();
          });
          system.then([output,pair,finish](const Json* value,
              const std::exception_ptr& failure){
            if(pair->failed)return;
            if(failure){pair->failed=true;output.reject(failure);return;}
            pair->system=*value;finish();
          });
        }catch(...){output.reject(std::current_exception());}
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
Json monsterConfig(const application::Config& config){
  const auto path=application::configurationDirectory(config)/"settings.json";
  std::ifstream stream(path);
  if(!stream)return Json::object();
  auto value=Json::parse(stream);
  if(!value.is_object())throw std::runtime_error("Monster configuration must be an object");
  return value;
}
std::shared_ptr<commands_indigenous::ClearServices> makeClear(Inputs& input){
  if(!input.hooks||!input.permission||!input.messages||!input.fileStash||
      !input.compactor||
      !input.managedShell.shell||!input.managedShell.poller||!input.managedShell.missions||
      !input.batTranscriptPath)
    return {};
  auto clear=std::make_shared<commands_indigenous::ClearServices>(input.owner);
  auto* owner=&input.owner;
  auto* view=&input.view;
  auto pendingId=std::make_shared<std::string>();
  clear->flightEndHookTimeoutMs=[](){return 600000.0;};
  clear->flightEndHooks=[owner,hooks=input.hooks](double){
    event_loop::Deferred<event_loop::Unit> result(*owner);
    hooks->run("SessionEnd",Json{{"reason","clear"}},"clear").then(
      [result](const std::vector<Json>*,const std::exception_ptr& error){
        if(error)result.reject(error);else result.resolve({});});
    return result;};
  clear->flightStartHooks=[owner,hooks=input.hooks,
      updateWatchPaths=input.updateWatchPaths]{
    event_loop::Deferred<Json> result(*owner);
    hooks->run("SessionStart",Json{{"source","clear"}},"clear").then(
      [result,hooks,updateWatchPaths](const std::vector<Json>* events,
          const std::exception_ptr& error){
        if(error){result.reject(error);return;}
        try{
          auto collected=hooks->collectFlightStart(events?*events:
            std::vector<Json>{});
          if(updateWatchPaths&&!collected.watchPaths.empty())
            updateWatchPaths(std::move(collected.watchPaths));
          result.resolve(std::move(collected.messages));
        }
        catch(...){result.reject(std::current_exception());}
      });
    return result;};
  clear->getAppState=[store=input.store]{return store->getState();};
  clear->setAppState=[store=input.store](const auto& update){store->setState(update);};
  clear->setMessages=[runtime=input.getRuntime,shell=input.getShell](Json values){
    auto live=runtime();if(!live)throw std::logic_error("Clear requires live runtime");
    auto messages=std::make_shared<interactive::TurnMessages>();
    if(values.is_array())for(auto& value:values)
      messages->push_back(std::make_shared<Json>(std::move(value)));
    live->messages().setMessages(std::move(messages));
    if(auto view=shell())view->messagesChanged();};
  clear->setConversationId=[view,shell=input.getShell](const std::string& value){
    view->conversationId=value;++view->revision;
    if(auto current=shell())current->requestRedraw();};
  clear->randomUUID=input.messages->randomUUID;
  clear->getOriginalCwd=[cwd=input.config.cwd]{return cwd.string();};
  clear->getFlightId=[flight=input.flight,pendingId]{
    return pendingId->empty()?flight->id():*pendingId;};
  clear->feature=[](const std::string&){return false;};
  clear->setContextBlocked=[view](bool blocked){view->contextBlocked=blocked;};
  auto stashes=std::make_shared<commands_indigenous::StashServices>(input.owner);
  const auto noop=[]{};
  stashes->clearUserContext=[poetry=input.poetry]{poetry->contextStash().clearUserContextStash();};
  stashes->clearSystemContext=[poetry=input.poetry]{poetry->contextStash().clearSystemContextStash();};
  stashes->clearGitStatus=[poetry=input.poetry]{poetry->clearGitStatus();};
  stashes->clearFlightStartDate=noop;stashes->clearFileSuggestions=noop;
  stashes->clearCommands=[skills=input.skills]{if(skills)skills->clearStashes();};
  stashes->resetPoetryStashBreakDetection=noop;
  stashes->runPostCompactCleanup=[compactor=input.compactor]{
    compactor->postCompactCleanup();};
  stashes->resetSentSkillNames=noop;
  stashes->clearStoredImagePaths=noop;stashes->clearAllFlights=noop;
  stashes->clearPendingPermissionCallbacks=[permission=input.permission]{permission->cancelAll();};
  stashes->clearRepositoryStashes=noop;stashes->clearBashCommandPrefixes=noop;
  stashes->clearDumpState=noop;stashes->clearGitDirResolution=noop;
  stashes->clearDynamicSkills=noop;stashes->resetLspDiagnostics=noop;
  stashes->clearTrackedDocuments=noop;stashes->clearFlightEnvironmentVariables=noop;
  stashes->setSystemPoetryInjection=[](std::nullptr_t){};
  stashes->setLastEmittedDate=[](std::nullptr_t){};
  stashes->resetMemoryFilesStash=[poetry=input.poetry](const std::string&){
    poetry->clearProjectInstructions();};
  stashes->clearInvokedSkills=[compactor=input.compactor](
      const std::vector<std::string>& preserved){
    compactor->clearInvokedSkills(preserved);};
  stashes->localUsageClearEnabled=[](){return false;};
  stashes->attributionEnabled=[](){return false;};
  stashes->clearLocalUsage=noop;stashes->clearAttributionStashes=noop;
  stashes->clearWebFetchStash=noop;stashes->clearGadgetSearchDescriptions=noop;
  stashes->clearBatDefinitions=noop;stashes->clearSkillPoetryStash=noop;
  commands_indigenous::bindClearFlightStashes(*clear,std::move(stashes));
  auto clearSourceStashes=std::move(clear->clearFlightStashes);
  clear->clearFlightStashes=[clearSourceStashes=std::move(clearSourceStashes),
      hooks=input.hooks,flight=input.flight](
      const std::vector<std::string>& preserved){
    clearSourceStashes(preserved);
    hooks->clearFlightHooks(flight->id());
    bats::saveStashSafeParams({});
  };
  clear->setCwd=[shell=input.managedShell.shell](const std::string& cwd){shell->setCwd(cwd);};
  clear->readFileState=input.fileStash;
  clear->discoveredSkillNames=input.discoveredSkills;
  clear->loadedNestedMemoryPaths=input.loadedMemoryPaths;
  clear->createEmptyAttributionState=[](){return Json::object();};
  clear->isLocalBatMission=bats::isLocalBatMission;
  clear->isInProcessPartnerMission=[](const Json& mission){
    return value::property(mission,"type")=="in_process_teammate";};
  clear->killForegroundMission=input.killForegroundMission?input.killForegroundMission:
    std::function<void(const std::string&,const Json&)>(
      [missions=input.managedShell.missions](const std::string& id,const Json& mission){
        if(value::property(mission,"type")=="local_agent")
          throw std::runtime_error("Agent task stop provider is unavailable");
        (void)missions->stop(id);});
  clear->evictMissionOutput=[poller=input.managedShell.poller](const std::string& id){
    poller->unregisterOutput(id);};
  clear->clearAllPlanSlugs=noop;
  clear->clearFlightMetadata=[flight=input.flight]{
    flights::MetadataServices imports;
    imports.project=&flight->project()->state();
    imports.paths=flight->paths().get();
    imports.getFlightId=[flight]{return flight->id();};
    flights::MetadataOperations(std::move(imports)).clearFlightMetadata();};
  clear->regenerateFlightId=[pendingId,uuid=input.messages->randomUUID](bool){
    *pendingId=uuid();};
  clear->internalFlightEnvPresent=[](){return std::getenv("BATBOX_SESSION_ID")!=nullptr;};
  clear->setEnvironmentFlightId=[](const std::string& id){
#ifdef _WIN32
    _putenv_s("BATBOX_SESSION_ID",id.c_str());
#else
    setenv("BATBOX_SESSION_ID",id.c_str(),1);
#endif
  };
  clear->resetFlightFilePointer=[owner,flight=input.flight,pendingId,
      getRuntime=input.getRuntime]{
    if(pendingId->empty())throw std::logic_error("Clear has no new session ID");
    auto live=getRuntime?getRuntime():nullptr;
    if(!live)return flight->resetToNewId(*pendingId);
    event_loop::Deferred<event_loop::Unit> result(*owner);
    live->awaitTranscriptIdle().then([flight,pendingId,result](
        const event_loop::Unit*,const std::exception_ptr& error){
      if(error){result.reject(error);return;}
      flight->resetToNewId(*pendingId).then([result](
          const event_loop::Unit*,const std::exception_ptr& resetError){
        if(resetError)result.reject(resetError);else result.resolve({});
      });
    });
    return result;};
  clear->getBatTranscriptPath=input.batTranscriptPath;
  clear->initMissionOutputAsSymlink=[state=input.config.stateDirectory,
      log=input.onError](
      const std::string& id,const std::string& target){
    (void)bats::initializeMissionOutputAsSymlink(state/"tasks"/(id+".output"),
      runtime::pathFromUtf8(target),[log](const std::exception&){
        if(log)log(std::current_exception());
      });};
  clear->isCoordinatorMode=[](){return false;};
  clear->saveMode=[](const std::string&){};
  clear->getCurrentWorktreeFlight=[]()->std::optional<Json>{return {};};
  clear->saveWorktreeState=[](const Json&){};
  clear->logError=input.onError?input.onError:[](std::exception_ptr){};
  return clear;
}
std::shared_ptr<commands_indigenous::CompactServices> makeCompact(Inputs& input){
  if(!input.compactor||!input.getBatContext||!input.messages)return {};
  auto compact=std::make_shared<commands_indigenous::CompactServices>(input.owner);
  auto* owner=&input.owner;
  auto* view=&input.view;
  compact->getMessages=[view]{
    Json values=Json::array();
    for(const auto& message:view->messages)if(message)values.push_back(*message);
    return values;};
  compact->afterCompactBoundary=[messages=input.messages](const Json& values){
    messages::BoundaryServices boundary;
    boundary.randomUUID=messages->randomUUID;boundary.nowISO=messages->nowISO;
    boundary.feature=messages->featureGate;
    boundary.projectSnippedView=[](const Json& value){return value;};
    return messages::getMessagesAfterCompactBoundary(values,false,boundary);};
  compact->aborted=[get=input.getBatContext]{
    const auto bat=get();return bat&&bat->query.stop.stop_requested();};
  compact->reactiveOnly=[](){return false;};
  compact->feature=[](const std::string&){return false;};
  compact->tryFlightMemoryCompaction=[owner](const Json&,
      const std::optional<std::string>&){
    return ready(*owner,std::optional<Json>{});};
  compact->microcompactMessages=[model=input.model,get=input.getBatContext](
      const Json& values){
    const auto bat=get();
    if(!bat)throw std::runtime_error("Compaction requires a completed query context");
    return model->microcompactMessages(values,bat->query,"compact");};
  compact->stashSharingParams=[owner,poetry=input.poetry,
      get=input.getBatContext](const Json& values){
    return freshCompactContext(*owner,poetry,get,values);};
  compact->compactConversation=[compactor=input.compactor,get=input.getBatContext](
      const Json& values,const Json& stash,const std::string& instructions){
    const auto bat=get();
    if(!bat)throw std::runtime_error("Compaction requires a completed query context");
    execution::Context current=bat->query;
    current.data["messages"]=values;
    auto snapshot=bats::makeStashSafeSnapshot(bat,
      stash.at("systemPrompt"),stash.at("userContext"),
      stash.at("systemContext"),current,values);
    return compactor->compact(values,bat->query,stash,false,
      instructions.empty()?std::nullopt:std::optional(instructions),
      std::move(snapshot));};
  compact->onCompactProgress=[view,shell=input.getShell](const Json& progress){
    view->streamMode=value::property(progress,"type")=="compact_end"
      ?std::optional<std::string>{}:std::optional<std::string>("compacting");
    ++view->revision;if(auto active=shell())active->requestRedraw();};
  compact->setStreamMode=[view,shell=input.getShell](const std::string& mode){
    view->streamMode=mode;++view->revision;
    if(auto active=shell())active->requestRedraw();};
  compact->resetResponseLength=[view]{view->responseLengthUtf16=0;};
  compact->clearUserContextStash=[poetry=input.poetry]{
    poetry->contextStash().clearUserContextStash();};
  compact->runPostCompactCleanup=[compactor=input.compactor]{
    compactor->postCompactCleanup();};
  compact->markPostCompaction=[]{};
  compact->suppressCompactWarning=[]{};
  compact->clearLastSummarizedMessageId=[]{};
  compact->batId=[]()->std::optional<std::string>{return {};};
  compact->querySource=[](){return std::string("compact");};
  compact->verbose=[](){return false;};
  compact->getUpgradeTip=[](){return std::string();};
  compact->getExpandShortcut=[](){return std::string("Ctrl+O");};
  compact->dim=[](const std::string& value){return value;};
  compact->logError=input.onError?input.onError:[](std::exception_ptr){};
  return compact;
}

}
std::shared_ptr<Bundle> Bundle::create(Inputs inputs){
  if(!inputs.owner.isOwnerThread())
    throw std::logic_error("Command bundle requires owner loop");
  if(!inputs.store||!inputs.model||!inputs.poetry||!inputs.flight||
      !inputs.getShell||!inputs.getRuntime)
    throw std::invalid_argument("Command bundle requires live application owners");
  auto self=std::shared_ptr<Bundle>(new Bundle(std::move(inputs)));
  self->initialize();return self;
}
void Bundle::initialize(){
  auto weak=weak_from_this();
  user_input::UiCommandRouter::Projection projection;
  projection.onUiChanged=[weak](std::optional<interactive::UiRequest> request){
    if(auto self=weak.lock()){
      self->inputs_.view.modal=std::move(request);
      ++self->inputs_.view.revision;
      if(auto shell=self->inputs_.getShell())shell->requestRedraw();
    }
  };
  projection.onError=[weak](const std::string& error){
    if(auto self=weak.lock()){
      if(self->inputs_.log)self->inputs_.log(error);
      if(self->inputs_.onError)self->inputs_.onError(
        std::make_exception_ptr(std::runtime_error(error)));
    }
  };
  projection.onQueueBlocked=[weak](bool){
    if(auto self=weak.lock())if(auto runtime=self->inputs_.getRuntime())
      runtime->queue().recheck();
  };
  projection.onPoetryFocus=[weak](bool focused){
    if(auto self=weak.lock()){
      self->inputs_.view.input.focused=focused;
      if(auto shell=self->inputs_.getShell())shell->setCommandFocus(focused);
    }
  };
  router_=user_input::UiCommandRouter::create(inputs_.owner,std::move(projection));
  auto catalog=std::make_shared<std::vector<Json>>();
  entries_.push_back(commands_indigenous::makeHelpCommand(router_,[catalog]{return *catalog;}));
  if(inputs_.requestExit){
    auto exit=std::make_shared<commands_indigenous::ExitServices>(inputs_.owner);
    exit->backgroundFlightsEnabled=[]{return false;};
    exit->isBackgroundFlight=[]{return false;};
    exit->hasCurrentWorktree=[]{return false;};
    exit->randomGoodbye=[]{return std::string("See you soon.");};
    exit->gracefulShutdown=[owner=&inputs_.owner,request=inputs_.requestExit](
        int,const std::string&){
      event_loop::Deferred<event_loop::Unit> result(*owner);
      try{request();result.resolve({});}
      catch(...){result.reject(std::current_exception());}
      return result;
    };
    entries_.push_back(commands_indigenous::makeExitCommand(router_,std::move(exit)));
  }
  if(inputs_.store&&inputs_.managedShell.missions&&inputs_.managedShell.shell){
    application_mission_ui::Inputs missions(inputs_.owner);
    missions.store=inputs_.store;
    missions.missionStore=std::make_shared<mission_management::Store>(
      application::configurationDirectory(inputs_.config));
    missions.missionListId=[flight=inputs_.flight]{
#ifdef _WIN32
      const auto value=runtime::environmentUtf8("BATBOX_TASK_LIST_ID");
      if(!value.empty())return value;
#else
      if(const auto* value=std::getenv("BATBOX_TASK_LIST_ID");value&&*value)
        return std::string(value);
#endif
      return flight->id();
    };
    missions.shellMissions=inputs_.managedShell.missions;
    missions.managedShell=inputs_.managedShell.shell;
    missions.stopBatMission=inputs_.stopBatMission;
    missions.openBatConversation=inputs_.openBatConversation;
    missions.batDefinitions=inputs_.batDefinitions;
    missions.dismissBatMission=inputs_.dismissBatMission;
    missions.reloadBatDefinitions=inputs_.reloadBatDefinitions;
    missions.getShell=inputs_.getShell;
    missions.terminalRows=[shell=inputs_.getShell]{
      if(auto current=shell())return current->viewState().rows;
      return 24;};
    missions.onError=inputs_.onError;
    entries_.push_back(application_mission_ui::makeMissionsCommand(router_,missions));
    if(missions.openBatConversation)
      entries_.push_back(application_mission_ui::makeBatsCommand(router_,std::move(missions)));
  }
  if(inputs_.memory)entries_.push_back(makeMemoryCommand(inputs_.owner,router_,
    inputs_.memory,inputs_.getShell));
  entries_.push_back(makeProjectCommand(inputs_.owner,inputs_.config.cwd,
    [poetry=inputs_.poetry]{if(poetry)poetry->clearProjectInstructions();}));
  application_usage::Sources usage;
  usage.config=inputs_.config;usage.store=inputs_.store;usage.view=&inputs_.view;
  usage.model=inputs_.model;usage.poetry=inputs_.poetry;
  usage.compactionPolicy=inputs_.compactionPolicy;
  usage.getBatContext=inputs_.getBatContext;usage.getShell=inputs_.getShell;
  usage.flightId=[flight=inputs_.flight]{return flight?flight->id():std::string{};};
  usage.runtimeMemory=inputs_.runtimeMemory;usage.interactive=!inputs_.config.print;
  entries_.push_back(application_usage::makeContextCommand(inputs_.owner,router_,usage));
  entries_.push_back(application_usage::makeStatusCommand(inputs_.owner,router_,usage));
  entries_.push_back(application_config::makeConfigCommand(inputs_.owner,router_,
    inputs_.config,inputs_.model,inputs_.store,inputs_.getShell,inputs_.compactionPolicy));
  entries_.push_back(makePermissionsCommand(inputs_.owner,router_,inputs_.config,
    inputs_.store,inputs_.flightTransitions));
  auto model=std::make_shared<commands_indigenous::ModelServices>(inputs_.owner);
  model->getState=[weak]{
    auto self=weak.lock();
    if(!self)throw std::runtime_error("Command bundle has retired");
    const auto state=self->inputs_.store->getState()->snapshot();
    commands_indigenous::ModelState result;
    const auto base=value::property(state,"mainLoopModel");
    const auto flight=value::property(state,"mainLoopModelForSession");
    if(base.is_string())result.baseModel=base.get<std::string>();
    if(flight.is_string())result.flightModel=flight.get<std::string>();
    const auto effort=value::property(state,"effortLevel");
    if(effort.is_string())result.effort=effort.get<std::string>();
    result.fastMode=value::property(state,"fastMode")==true;
    return result;};
  model->setModel=[store=inputs_.store](std::optional<std::string> chosen){
    store->setState([chosen](const app_state::AppStateRef& previous){
      auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
      next->fields->set("mainLoopModel",chosen?Json(*chosen):Json());
      next->fields->set("mainLoopModelForSession",nullptr);
      return next;});};
  model->setFastMode=[store=inputs_.store](bool enabled){
    store->setState([enabled](const app_state::AppStateRef& previous){
      auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
      next->fields->set("fastMode",enabled);return next;});};
  model->renderDefaultModelSetting=[](const std::string& value){return value;};
  model->renderModelName=[](const std::string& value){return value;};
  model->mainLoopModel=[weak]{
    auto self=weak.lock();
    if(!self)throw std::runtime_error("Command bundle has retired");
    return selected(self->inputs_);
  };
  model->defaultModelSetting=[value=inputs_.config.model]{return value;};
  model->choices=[backend=inputs_.model,defaultModel=inputs_.config.model]{
    std::vector<commands_indigenous::ModelChoice> result;
    result.push_back({"Default ("+defaultModel+")",std::nullopt,std::nullopt});
    for(const auto& [id,profile]:backend->config().profiles){
      const std::string label=profile.wireModel.empty()||profile.wireModel==id
        ?id:id+" ("+profile.wireModel+")";
      result.push_back({label,id,std::nullopt});}
    return result;};
  model->isModelAllowed=[backend=inputs_.model](const std::string& id){
    return backend->config().profiles.contains(id);};
  model->premiumExtendedContextUnavailable=[](const std::string&){return false;};
  model->standardExtendedContextUnavailable=[](const std::string&){return false;};
  model->isKnownAlias=model->isModelAllowed;
  model->validateModel=[backend=inputs_.model,owner=&inputs_.owner](const std::string& id){
    event_loop::Deferred<Json> result(*owner);
    if(backend->config().profiles.contains(id))result.resolve({{"valid",true}});
    else result.resolve({{"valid",false},{"error","Model '"+id+"' is not configured"}});
    return result;};
  model->isFastModeEnabled=[](){return false;};
  model->isFastModeAvailable=[](){return false;};
  model->isFastModeSupportedByModel=[](const std::optional<std::string>&){return false;};
  model->isExtendedContextMergeEnabled=[](){return false;};
  model->billedAsExtraUsage=[](const std::optional<std::string>&,bool,bool){return false;};
  model->clearFastModeCooldown=[]{};
  model->immediate=[](){return false;};
  model->bold=[](const std::string& value){return value;};
  entries_.push_back(application_config::makeRoutedModelCommand(
    commands_indigenous::makeModelCommand(router_,std::move(model)),
    inputs_.owner,router_,inputs_.config,inputs_.model,inputs_.store,inputs_.getShell));
  if(inputs_.mcpRuntime)
    entries_.push_back(makeMcpCommand(router_,inputs_.mcpRuntime,
      inputs_.mcpAuthUi));
  if(inputs_.mcpRuntime)for(const auto& server:
      inputs_.mcpRuntime->manager()->snapshot()){
    if(server.type!="connected"||!server.commands.is_array())continue;
    for(const auto& poetry:server.commands)
      entries_.push_back(makeMcpPoetryCommand(inputs_.mcpRuntime,
        inputs_.mcpPoetryResults,inputs_.mcpPoetryAttachments,
        inputs_.messages,inputs_.invokedSkills,inputs_.getBatContext,
        inputs_.owner,poetry));
  }
  if(inputs_.skills){
    auto skillCommands=inputs_.skills->slashCommands();
    for(auto& command:skillCommands)entries_.push_back(std::move(command));
  }
  auto monsterServices=std::make_shared<commands_indigenous::MonsterServices>();
  monster::Services rollServices;
  rollServices.getGlobalConfig=[config=inputs_.config]{return monsterConfig(config);};
  monsterServices->roller=std::make_shared<monster::Roller>(
    std::move(rollServices));
  entries_.push_back(commands_indigenous::makeMonsterCommand(router_,
    std::move(monsterServices)));
  if(!inputs_.clear)inputs_.clear=makeClear(inputs_);
  if(inputs_.clear)entries_.push_back(commands_indigenous::makeClearCommand(inputs_.clear));
  if(!inputs_.compact)inputs_.compact=makeCompact(inputs_);
  if(inputs_.compact)entries_.push_back(commands_indigenous::makeCompactCommand(inputs_.compact));
  if(inputs_.resume)entries_.push_back(commands_indigenous::makeResumeCommand(router_,inputs_.resume));
  if(inputs_.openRewind){
    auto rewind=std::make_shared<commands_indigenous::RewindServices>(inputs_.owner);
    rewind->openMessageSelector=inputs_.openRewind;
    entries_.push_back(commands_indigenous::makeRewindCommand(std::move(rewind)));
  }
  catalog->reserve(entries_.size());
  for(const auto& entry:entries_)catalog->push_back(entry.descriptor);
}
bool Bundle::backgroundForegroundMissions(){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("Task backgrounding requires owner loop");
  bool changed=false;
  if(inputs_.managedShell.missions){
    changed=!inputs_.managedShell.missions->backgroundAllForeground().empty();
  }
  if(inputs_.store&&inputs_.backgroundBatMission){
    const auto snapshot=inputs_.store->getState()->snapshot();
    const auto& missions=value::property(snapshot,"tasks");
    if(missions.is_object())for(auto it=missions.begin();it!=missions.end();++it){
      const auto& mission=it.value();
      if(value::property(mission,"type")!="local_agent"||
         value::property(mission,"status")!="running"||
         value::property(mission,"isBackgrounded")!=false||
         value::property(mission,"agentType")=="main-session")continue;
      changed=inputs_.backgroundBatMission(it.key())||changed;
    }
  }
  return changed;
}
void Bundle::openMissions(const std::string& id){
  (void)openUiCommand("tasks",id);
}
bool Bundle::openUiCommand(const std::string& name,const std::string& args){
  if(router_->ui()->busy()||router_->ui()->hasActionableUi())return false;
  for(const auto& entry:entries_)if(value::property(entry.descriptor,"name")==name){
    if(!entry.invokeUi)return false;
    entry.invokeUi(args).then([report=inputs_.onError](
        const user_input::UiCompletion*,const std::exception_ptr& error){
      if(error&&report)report(error);
    });
    return true;
  }
  return false;
}
} // namespace batbox::application_commands
