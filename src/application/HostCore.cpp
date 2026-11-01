#include "application/HostInternal.hpp"
#include "application/ModelBinding.hpp"
#include "application/Async.hpp"
#include "application_mcp_elicitation/ModelBinding.hpp"
#include "runtime/Platform.hpp"
#include "flights/ProjectDirectories.hpp"
#include "shell/BashProvider.hpp"
#include "shell/WindowsPaths.hpp"
#include "runtime/IndigenousPath.hpp"
#include "interactive/background/NotificationAttachments.hpp"
#include "gadgets/Gadget.hpp"
#ifdef _WIN32
#include "runtime/IndigenousUnicode.hpp"
#endif
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace batbox::application {
namespace {
std::int64_t nowMs(){return platform::nowMillis();}
std::string errorText(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::exception& failure){return failure.what();}
  catch(...){return "unknown native application error";}
  return {};
}
std::shared_ptr<shell_provider::BashProvider> makeShellProvider(
    const std::string& shellPath){
  shell_provider::Services services;
  services.skipSnapshot=true;
  services.flightEnvironmentScript=[]()->std::optional<std::string>{return {};};
  services.flightEnvVars=[](){return std::map<std::string,std::string>{};};
  services.tmuxGadgetUsed=[] {return false;};
  services.ensureTmuxSocket=[] {};
  services.tmuxEnvironment=[]()->std::optional<std::string>{return {};};
  services.operatingSystemTempDir=[](){return runtime::pathToUtf8(
    std::filesystem::temp_directory_path());};
  services.snapshotAccessible=[](const std::string& path){return std::filesystem::exists(
    runtime::pathFromUtf8(path));};
#ifdef _WIN32
  services.windows=true;
  services.indigenousToPosixPath=shell::windowsPathToPosixPath;
  services.indigenousJoin=[](const std::string& base,const std::string& suffix){
    return runtime::pathToUtf8(runtime::pathFromUtf8(base)/
      runtime::pathFromUtf8(suffix));};
#endif
  return std::make_shared<shell_provider::BashProvider>(shellPath,std::move(services));
}
Json initialPermissions(const Config& config){
  Json result=gadgets::getEmptyGadgetPermissionContext();
  if(config.permissions.is_object())for(auto it=config.permissions.begin();
      it!=config.permissions.end();++it)result[it.key()]=it.value();
  for(const auto& [setting,contextKey]:{
      std::pair{"allow","alwaysAllowRules"},
      std::pair{"deny","alwaysDenyRules"},
      std::pair{"ask","alwaysAskRules"}}){
    const auto& configured=value::property(config.permissions,setting);
    if(!configured.is_array())continue;
    auto& rules=result[contextKey];
    if(rules.is_array())rules=Json{{"configured",rules}};
    if(!rules.is_object())rules=Json::object();
    rules["settings"]=configured;
  }
  if(config.permissions.is_object()&&config.permissions.contains("defaultMode"))
    result["mode"]=config.permissions.at("defaultMode");
  if(result["mode"]=="bypassPermissions"&&
      (!config.permissions.is_object()||
       !config.permissions.contains("disableBypassPermissionsMode")||
       config.permissions.at("disableBypassPermissionsMode")!="disable"))
    result["isBypassPermissionsModeAvailable"]=true;
  return result;
}
}
Host::Host(Config config):impl_(std::make_unique<Impl>(std::move(config))){}
Host::~Host(){
  if(!impl_)return;
  // Initialization errors and embedders can unwind without entering run()'s
  // normal exit path. Settle provider shutdown while every host field exists.
  try{
    impl_->requestExit();
    impl_->owner.run();
    impl_->shutdown();
    impl_->owner.runReady();
  }catch(...){
    // Destruction is noexcept; drain already-settled callbacks even when a
    // failed event-loop callback prevented the ordinary shutdown sequence.
    impl_->shutdown();
    try{impl_->owner.runReady();}catch(...){}
  }
  // The process-wide fork stash may retain callbacks into this owner loop.
  // Release it while the host and its event loop still exist.
  bats::saveStashSafeParams({});
}
void Host::Impl::log(std::string value)const{
  if(logFile&&*logFile){
    std::lock_guard lock(*logMutex);
    *logFile<<platform::isoTimestamp()<<" "<<value<<'\n';logFile->flush();
  }
}
void Host::Impl::report(std::exception_ptr error){
  const auto value=errorText(error);
  if(!value.empty())log(value);
  if(value.empty())return;
  failed.store(true);
  const auto show=[this,value]{
    if(closing)return;
    statusText="Error: "+value;
    ++view.revision;
    if(shell)shell->requestRedraw();
    if(config.print)std::cerr<<"Batbox: "<<value<<'\n';
  };
  if(owner.isOwnerThread())show();
  else owner.postMission(show);
}
gadgets::Gadgets Host::Impl::currentGadgets()const{
  const auto snapshot=roster?roster->current():nullptr;
  if(!snapshot)return {};
  gadgets::Gadgets result=snapshot->base;
  result.insert(result.end(),snapshot->mcp.begin(),snapshot->mcp.end());
  return result;
}
Json Host::Impl::appState()const{
  return store->getState()->snapshot();
}
void Host::Impl::initialize(){
  std::filesystem::current_path(config.cwd);
  std::filesystem::create_directories(config.stateDirectory);
  roost=std::make_shared<application_roost::Roost>(owner,config.cwd);
  config.cwd=roost->cwd();
  flightId=platform::randomUUID();
  logMutex=std::make_shared<std::mutex>();
  logFile=std::make_shared<std::ofstream>(
    config.stateDirectory/"native.log",std::ios::app);
  messages=std::make_shared<messages::Services>();
  messages->randomUUID=platform::randomUUID;
  messages->nowISO=[] {return platform::isoTimestamp();};
  messages->strictGadgetResultPairing=[] {return true;};
  messages->featureGate=[](const std::string&){return false;};
  messages->logError=[this](const std::string& value){log(value);};
  app_state::DefaultAppStateServices defaults;
  defaults.isPartner=[] {return false;};
  defaults.isPlanModeRequired=[] {return false;};
  defaults.getInitialSettings=[this]{
    Json result=config.settings;
    result["permissions"]=config.permissions;result["hooks"]=config.hooks;
    result["worktree"]=config.worktree;return result;
  };
  defaults.getEmptyGadgetPermissionContext=[this]{return initialPermissions(config);};
  defaults.createEmptyAttributionState=[] {return Json::object();};
  defaults.shouldEnableThinkingByDefault=[enabled=config.thinking]{return Json(enabled);};
  defaults.shouldEnablePoetrySuggestion=[] {return Json(false);};
  defaults.idleSpeculationState=std::make_shared<Json>(Json::object());
  auto state=app_state::getDefaultAppState(defaults);
  auto permissionContext=initialPermissions(config);
  state->setPermissionContext(std::make_shared<value::SharedRecord>(
    value::SharedRecord::fromJson(permissionContext)));
  store=std::make_shared<app_state::AppStateStore>(std::move(state));
  compactionPolicy=application_compaction_settings::Policy::create(owner,config,store);
  application_mcp_elicitation::Inputs elicitationInputs(owner);
  elicitationInputs.interactive=!config.print;
  elicitationInputs.diagnostic=[this](const std::string& message){log(message);};
  elicitationInputs.openUrl=[this](const std::string& url){
    runInWorker<bool>(owner,[this,url]{return mcp::auth::openBrowser(owner,url);})
      .then([this](const bool* opened,const std::exception_ptr& error){
        if(error||!opened||!*opened)log("Open the MCP link in your browser to continue.");
      });
  };
  elicitationInputs.parseNaturalDateTime=[this](const std::string& input,
      const std::string& format,std::stop_token signal){
    return application_mcp_elicitation::parseNaturalLanguageDateTime(owner,input,format,
      signal,application_mcp_elicitation::makeDateTimeQuery(owner,model),
      [this](std::exception_ptr error){log("Date parsing: "+errorText(error));});
  };
  mcpElicitation=application_mcp_elicitation::Controller::create(std::move(elicitationInputs));
  fileStash=file_state_stash::createFileStateStashWithSizeLimit(
    file_state_stash::READ_FILE_STATE_STASH_SIZE);
  replacements=gadget_results::provisionContentReplacementState(false);
  loadedMemoryPaths=std::make_shared<runtime::OrderedStringSet>();
  discoveredSkills=std::make_shared<runtime::OrderedStringSet>();
  invokedSkills=std::make_shared<bootstrap::InvokedSkillsState>();
  roster=std::make_shared<gadgets::live_roster::Roster>();
  application_flights::Inputs flightInputs(owner);
  flightInputs.config=config;flightInputs.roost=roost;
  flightInputs.flightId=flightId;
  flightInputs.logError=[this](std::exception_ptr error){report(error);};
  flightInputs.commandNames=[] {return std::set<std::string>{};};
  flight=application_flights::Flight::create(std::move(flightInputs));
  bindFileHistory();
  application_media::Inputs mediaInputs(owner);
  mediaInputs.stateDirectory=config.stateDirectory;
  mediaInputs.flightId=[flight=flight]{return flight->id();};
  mediaInputs.log=[this](const std::string& value){log(value);};
  mediaInputs.onError=[this](std::exception_ptr error){report(error);};
  mediaInputs.terminalName=[] {
    const char* name=std::getenv("TERM");return name&&*name?std::string(name):std::string{};};
  media=application_media::Bundle::create(std::move(mediaInputs));
  application_history::Inputs historyInputs(owner);
  historyInputs.stateDirectory=config.stateDirectory.string();
  historyInputs.projectRoot=[roost=roost]{
    return roost->projectRoot().string();};
  historyInputs.flightId=[flight=flight]{return flight->id();};
  historyInputs.log=[this](const std::string& value){log(value);};
  historyInputs.onError=[this](std::exception_ptr error){report(error);};
  historyInputs.skipPoetryHistory=[] {return false;};
  history=application_history::Bundle::create(std::move(historyInputs));
  bindStorage();
  model=makeLocalModel(owner,config,messages,
    [this](const std::string& value){log(value);},roster);
  connectors=application_connectors::bootstrap(owner,config,{},
    [this](const std::string& server,const std::string& message){
      log("MCP "+server+": "+message);},true);
  application_permissions::PermissionProvider::Inputs permissionInputs(owner);
  permissionInputs.interactive=!config.print;
  permissionInputs.workingDirectory=[roost=roost]{return runtime::pathToUtf8(roost->cwd());};
  permissionInputs.homeDirectory=[]{
#ifdef _WIN32
    auto home=runtime::environmentUtf8("USERPROFILE");
    if(home.empty())home=runtime::environmentUtf8("HOME");
#else
    const char* value=std::getenv("HOME");
    std::string home=value?value:"";
#endif
    return home;
  };
  permission=application_permissions::PermissionProvider::create(
    std::move(permissionInputs));
  bindRoostProviders();
  bindBackend();
  bindMcpRuntime();
  bindFrontend();
}
void Host::Impl::bindStorage(){
  auto directories=std::make_shared<flights::ProjectDirectories>(
    [state=config.stateDirectory]{return state.string();});
  gadget_results::StorageImports paths;
  paths.getOriginalCwd=[cwd=config.cwd]{return cwd.string();};
  paths.getFlightId=[flight=flight]{return flight->id();};
  paths.getProjectDir=[directories](const std::string& cwd){
    return directories->getProjectDir(cwd);};
  paths.logError=[this](std::exception_ptr error){report(error);};
  paths.debug=[this](const std::string& value){log(value);};
  storage=gadget_results::indigenousStorageImports(owner,std::move(paths));
}
void Host::Impl::bindFileHistory(){
  application_file_history::Inputs inputs(owner);
  inputs.store=store;inputs.flight=flight;
  inputs.originalCwd=config.cwd;
  inputs.configHome=config.stateDirectory;
  inputs.nonInteractive=config.print;
  inputs.configuredEnabled=[enabled=config.fileCheckpointingEnabled]{
    return enabled.value_or(true);};
  inputs.logError=[this](std::exception_ptr error){report(error);};
  fileHistory=application_file_history::History::create(std::move(inputs));
}
void Host::Impl::bindRoostProviders(){
  generationEngineSlot=std::make_shared<std::weak_ptr<application_engine::Engine>>();
  generationBatSlot=std::make_shared<std::weak_ptr<application_bats::IndigenousBatApplication>>();
  const std::weak_ptr<gadgets::live_roster::Roster> generation=roster;
  const auto generationSettings=value::property(appState(),"settings");
  application_skills::Inputs skillInputs(owner);
  skillInputs.config=config;
  skillInputs.projectRoot=bindingProjectRoot();
  skillInputs.messages=messages;skillInputs.flight=flight;
  skillInputs.model=model;skillInputs.invoked=invokedSkills;
  skillInputs.resultStorage=storage;
  skillInputs.currentGadgetSnapshot=[generation]{
    if(auto live=generation.lock())return live->current();
    return std::shared_ptr<const gadgets::live_roster::Snapshot>{};
  };
  skillInputs.currentMessages=[this,generation]{
    Json rows=Json::array();
    if(generation.lock()!=roster)return rows;
    for(const auto& message:view.messages)if(message)rows.push_back(*message);
    return rows;};
  skillInputs.mcpCommands=[this,generation]{return generation.lock()==roster?value::property(
    value::property(appState(),"mcp"),"commands"):Json::array();};
  skillInputs.appState=[this,generation,generationSettings]{
    auto state=appState();
    if(generation.lock()!=roster)state["settings"]=generationSettings;
    return state;};
  skillInputs.currentBat=[this,generation]{
    return generation.lock()==roster?currentBat:nullptr;};
  skillInputs.diagnostic=[this](const std::string& value){log(value);};
  skillInputs.changed=[this,generation]{
    if(auto live=generation.lock())refreshCommandCatalog(live);};
  skills=application_skills::Provider::create(std::move(skillInputs));
  application_poetry::Inputs poetryInputs;
  poetryInputs.config=config;
  poetryInputs.initialSettings=[this,generation,generationSettings]{
    return generation.lock()==roster?value::property(appState(),"settings"):generationSettings;};
  poetryInputs.skillCommands=[skills=skills]{return skills->modelCommands();};
  poetryInputs.outputStyle=[] {return Json();};
  poetryInputs.recoveryActivity=[this,generation](bool active){
    if(roster==generation.lock()&&shell)shell->setRemembering(active);
  };
  poetryInputs.logError=[this](std::exception_ptr error){report(error);};
  poetryInputs.logDiagnostic=[this](const std::string& scope,
      const std::string& value,const std::optional<Json>&){log(scope+": "+value);};
  poetry=std::make_shared<application_poetry::Provider>(owner,std::move(poetryInputs));
  application_shell::Inputs shellInputs;
  shellInputs.owner=&owner;
#ifdef _WIN32
  shellInputs.shellPath=shell::findGitBashPath();
#else
  const char* shellEnv=std::getenv("SHELL");
  shellInputs.shellPath=shellEnv&&*shellEnv?shellEnv:"/bin/sh";
#endif
  shellInputs.originalCwd=config.cwd.string();
  shellInputs.outputDir=(config.stateDirectory/"shell-output").string();
  std::filesystem::create_directories(shellInputs.outputDir);
  shellInputs.provider=makeShellProvider(shellInputs.shellPath);
  shellInputs.generateMissionId=platform::randomUUID;
  shellInputs.generateExecId=platform::randomUUID;
  shellInputs.nowMs=nowMs;
  shellInputs.enqueueNotification=[this](const std::string& text,
      const std::optional<std::string>&){log("Shell task: "+text);};
  shellInputs.onMissionTerminated=[this](const shell_missions::MissionRecord& mission){
    log("Shell task terminated: "+mission.id);};
  shellInputs.onBackgroundError=[this](std::exception_ptr error){report(error);};
  managedShell=application_shell::makeHost(std::move(shellInputs));
  application_hooks::Inputs hookInputs;
  hookInputs.config=config;hookInputs.flightId=flightId;
  hookInputs.flightIdProvider=[this]{
    return hookFlightOverride.value_or(flight->id());};
  hookInputs.transcriptPathForFlight=[flight=flight](const std::string& id){
    return flight->paths()->getTranscriptPathForFlight(id);};
  hookInputs.settings=value::property(appState(),"settings");
  hookInputs.getAppState=[this]{return appState();};
  hookInputs.model=model;hookInputs.messages=messages;
  const auto generationGadgets=[weak=std::weak_ptr(roster)]{
    gadgets::Gadgets result;
    if(auto roster=weak.lock())if(auto snapshot=roster->current()){
      result=snapshot->base;result.insert(result.end(),snapshot->mcp.begin(),snapshot->mcp.end());
    }
    return result;
  };
  hookInputs.currentGadgets=generationGadgets;
  hookInputs.currentContext=[this,weak=std::weak_ptr(roster),generationGadgets]{
    const auto generation=weak.lock();
    execution::Context context=generation==roster&&currentBat?currentBat->query:
      execution::Context{};
    if(context.gadgets.empty()){
      context.gadgets=generationGadgets();
      if(generation)context.gadgetSnapshotOwner=generation->current();
    }
    if(context.baseGadgets.empty())context.baseGadgets=context.gadgets;
    return context;};
  hookInputs.makeBatEngine=[slot=generationEngineSlot](const bats::BatContext& context,
      const std::function<std::future<Json>(const Json&)>& canUse){
    const auto engine=slot->lock();
    if(!engine)throw std::logic_error("Agent hook workspace engine is unavailable");
    return engine->forBat(context,canUse)->services();};
  hookInputs.batTranscriptPath=[slot=generationBatSlot](const std::string& id){
    const auto bats=slot->lock();
    if(!bats)throw std::logic_error("Agent hook transcript owner unavailable");
    return bats->transcriptPath(id);};
  hookInputs.setAppState=[slot=generationBatSlot](const std::function<Json(const Json&)>& update){
    const auto bats=slot->lock();
    if(!bats)throw std::logic_error("Agent hook state owner unavailable");
    bats->setAppState(update);};
  hookInputs.permission=permission;
  hookInputs.currentMessages=[this,weak=std::weak_ptr(roster)]{
    Json rows=Json::array();
    if(weak.lock()!=roster)return rows;
    if(runtime){
      const auto& current=runtime->messages().messages();
      if(current)for(const auto& message:*current)
        if(message)rows.push_back(*message);
    }else for(const auto& message:view.messages)
      if(message)rows.push_back(*message);
    return rows;};
  hookInputs.addResponseLength=[this](std::size_t size){
    if(runtime)runtime->loading().setResponseLength(
      [size](double value){return value+static_cast<double>(size);});
    else view.responseLengthUtf16+=size;};
  hookInputs.log=[this](const std::string& value){log(value);};
  hookInputs.randomUUID=platform::randomUUID;
  hookInputs.emitHookEvent=[this](const Json& value){
    log("Hook event: "+value.dump());};
  hookInputs.enqueuePendingNotification=[this](const Json& value){
    if(runtime)runtime->queue().enqueuePendingNotification(value);
    else log("Hook notification before runtime startup: "+value.dump());};
  hookInputs.currentAbortReason=[this,weak=std::weak_ptr(roster)]{
    if(weak.lock()!=roster)return std::string{};
    return currentBat&&currentBat->currentAbortReason?
      currentBat->currentAbortReason():std::string{};};
  hookInputs.createAttachmentMessage=[randomUUID=messages->randomUUID,
      nowISO=messages->nowISO](const Json& attachment){
    return *interactive::background::createAttachmentMessage(attachment,
      randomUUID,nowISO);
  };
  hooks=std::make_shared<application_hooks::Provider>(owner,std::move(hookInputs));
  if(fileWatcher)fileWatcher->close();
  application_hooks::FileChangedInputs fileInputs(owner);
  fileInputs.cwd=runtime::pathToUtf8(config.cwd);
  fileInputs.hooksConfig=[this]{return value::property(appState(),"settings");};
  fileInputs.fileChanged=[provider=hooks](const std::string& path,
      const std::string& event){return provider->fileChanged(path,event);};
  fileInputs.cwdChanged=[provider=hooks](const std::string& oldCwd,
      const std::string& newCwd){return provider->cwdChanged(oldCwd,newCwd);};
  fileInputs.clearCwdEnvFiles=[provider=hooks]{
    return provider->clearCwdEnvFiles();};
  fileInputs.notify=[this](const std::string& value,bool isError){
    statusText=value;++view.revision;
    if(isError)log("FileChanged hook: "+value);
    if(shell)shell->requestRedraw();};
  fileInputs.log=[this](const std::string& value){log(value);};
  fileWatcher=application_hooks::FileChangedWatcher::create(
    std::move(fileInputs));
  fileWatcher->start();
}
int Host::run(){
  auto& host=*impl_;
  host.initialize();
  host.runtime->start();
  host.shell->start();
  // Ask for the terminal background so the "auto" theme can follow it; the
  // answer arrives as an OSC 11 response.
  if(!host.config.print&&host.shell->driver().interactive())
    host.shell->driver().writeRaw("\x1b]11;?\x07");
  auto submitInitial=[&host]{
    const auto poetry=host.poetry;
    poetry->projectRecovery().then([&host,poetry](const std::string*,
        const std::exception_ptr& error){if(error&&!host.closing)host.report(error);});
    if(host.config.print){
      if(!host.cli)throw std::logic_error("Headless CLI adapter is unavailable");
      host.cli->start();return;
    }
    if(!host.config.initialPoetry)return;
    if(!host.runtime->runtime()->handleIncomingPoetry(*host.config.initialPoetry)){
      host.report(std::make_exception_ptr(std::runtime_error(
        "Initial prompt could not be submitted")));
    }
  };
  const auto startFresh=[&host,submitInitial]{
    if(!host.hooks->hasHooksFor("SessionStart")){
      submitInitial();return;
    }
    Json detail={{"source","startup"}};
    const auto selected=selectedInferenceProfile(host.config);
    if(!selected.model.empty())detail["model"]=selected.model;
    host.hooks->run("SessionStart",std::move(detail),"startup").then(
      [&host,submitInitial](const std::vector<Json>* events,
          const std::exception_ptr& error){
        if(error)host.report(error);
        if(events){
          const auto collected=host.hooks->collectFlightStart(*events);
          auto values=std::make_shared<interactive::TurnMessages>(
            *host.runtime->messages().messages());
          for(const auto& message:collected.messages)
            values->push_back(std::make_shared<Json>(message));
          if(values->size()!=host.runtime->messages().messages()->size())
            host.runtime->messages().setMessages(std::move(values));
          if(host.cli&&collected.initialUserMessage)
            host.cli->prependUserMessage(*collected.initialUserMessage);
          if(host.fileWatcher&&!collected.watchPaths.empty())
            host.fileWatcher->updateWatchPaths(collected.watchPaths);
        }
        if(!host.closing)submitInitial();
      });
  };
  const auto begin=[&host,submitInitial,startFresh]{
    if(host.config.resumePicker){
      startFresh();
      if(!host.commands||!host.commands->openUiCommand("resume"))
        host.report(std::make_exception_ptr(std::runtime_error(
          "The Flight picker could not be opened; use /resume")));
      return;
    }
    if(!host.config.resumeFlight&&!host.config.continueFlight){
      startFresh();return;
    }
    auto loaded=host.config.resumeFlight?
      host.restore->find(*host.config.resumeFlight):host.restore->latest();
    loaded.then([&host,submitInitial](const std::optional<Json>* log,
        const std::exception_ptr& error){
      if(error){host.report(error);if(host.config.print)host.requestExit();return;}
      if(!log||!*log){
        host.report(std::make_exception_ptr(std::runtime_error(
          host.config.resumeFlight?"No saved Flight matches \""+
            *host.config.resumeFlight+"\"":
            std::string("No saved Flight to continue in this directory"))));
        if(host.config.print)host.requestExit();
        return;
      }
      const auto& id=value::property(**log,"sessionId");
      if(!id.is_string()){
        host.report(std::make_exception_ptr(std::runtime_error(
          "Saved conversation has no session ID")));
        if(host.config.print)host.requestExit();
        return;
      }
      host.restore->resume(id.get<std::string>(),**log,"startup").then(
        [&host,submitInitial](const event_loop::Unit*,
            const std::exception_ptr& restoreError){
          if(restoreError){
            host.report(restoreError);
            if(host.config.print)host.requestExit();
          }else submitInitial();
        });
    });
  };
  host.owner.queueMicrotask([&host,begin]{
    if(!host.mcpRuntime){begin();return;}
    try{
      host.mcpRuntime->start().then([&host,begin](const event_loop::Unit*,
          const std::exception_ptr& error){
        if(error)host.log("MCP startup failed");
        begin();
      });
    }catch(...){host.log("MCP startup failed");begin();}
  });
  auto applicationLifetime=host.owner.hold();
  host.owner.run();
  applicationLifetime.release();
  host.shutdown();
  // stop() can leave settled promise observers queued. Release their provider
  // references before the event loop itself is destroyed.
  host.owner.runReady();
  return host.failed.load()?1:0;
}
}
