#include "application/HostInternal.hpp"
#include "application_commands/FlightTitles.hpp"
#include "interactive/presentation_providers/DefaultBindings.hpp"
#include "interactive/text_input/TextEditorIndigenous.hpp"
#include "runtime/MessageSnapshot.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/Platform.hpp"
#include "runtime/IndigenousUnicode.hpp"
#include "flights/Transcript.hpp"
#include "mcp/OutputStorage.hpp"
#include "gadget_results/Storage.hpp"
#include "user_input/CommandMessages.hpp"
#include "cli/headless/PermissionControl.hpp"
#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace batbox::application {
namespace {
std::string indigenousEnvironment(const char* name){
#ifdef _WIN32
  return runtime::environmentUtf8(name);
#else
  const auto* value=std::getenv(name);return value?value:"";
#endif
}
template<class T>event_loop::Deferred<T> unsupported(event_loop::Loop& owner,
    const std::string& message){
  event_loop::Deferred<T> result(owner);
  result.reject(std::make_exception_ptr(std::runtime_error(message)));
  return result;
}
std::string mode(const Json& state){
  const auto& value=value::property(value::property(state,"toolPermissionContext"),"mode");
  return value.is_string()?value.get<std::string>():"default";
}
}
std::shared_ptr<application_commands::Bundle> Host::Impl::makeCommands(){
  application_commands::Inputs commandInputs(owner,view);
  commandInputs.config=config;commandInputs.store=store;
  commandInputs.flightTransitions=flightTransitions;
  commandInputs.compactionPolicy=compactionPolicy;
  commandInputs.model=model;commandInputs.poetry=poetry;
  commandInputs.flight=flight;
  commandInputs.resume=restore->makeResumeServices();
  commandInputs.compactor=compactor;
  commandInputs.mcpRuntime=mcpRuntime;
  commandInputs.mcpAuthUi=mcpAuthUi;
  commandInputs.invokedSkills=invokedSkills;
  commandInputs.skills=skills;
  commandInputs.memory=memory;
  mcp::StorageConfig poetryStorage;
  poetryStorage.getGadgetResultsDir=[storage=storage]{
    return gadget_results::getGadgetResultsDir(storage);};
  poetryStorage.ensureGadgetResultsDir=[storage=storage]{
    std::filesystem::create_directories(gadget_results::getGadgetResultsDir(storage));};
  poetryStorage.nowMs=platform::nowMillis;
  poetryStorage.randomSuffix=messages->randomUUID;
  mcp::bindIndigenousResultServices(commandInputs.mcpPoetryResults,poetryStorage);
  commandInputs.mcpPoetryAttachments=[aggregate=attachments,view=&view](
      const std::string& content){
    Json all=Json::array();
    for(const auto& message:view->messages)if(message)all.push_back(*message);
    return aggregate->collectPoetry(content,Json(),Json::array(),all,
      std::optional<std::string>("repl_main_thread"));};
  commandInputs.getBatContext=[this]{return currentBat;};
  commandInputs.runtimeMemory=[this]{return runtimeMemorySummary();};
  commandInputs.hooks=hooks;
  commandInputs.permission=permission;
  commandInputs.messages=messages;
  commandInputs.fileStash=fileStash;
  commandInputs.managedShell=managedShell;
  commandInputs.discoveredSkills=discoveredSkills;
  commandInputs.loadedMemoryPaths=loadedMemoryPaths;
  commandInputs.batTranscriptPath=[bats=bats](const std::string& id){
    return bats->transcriptPath(id);};
  commandInputs.killForegroundMission=[bats=bats,missions=managedShell.missions](
      const std::string& id,const Json& mission){
    if(value::property(mission,"type")=="local_agent")bats->abortMission(id);
    else (void)missions->stop(id);};
  const auto missionOwner=[this](const std::string& id){
    return batOwner(id);
  };
  commandInputs.stopBatMission=[missionOwner](const std::string& id){missionOwner(id)->stopMission(id);};
  commandInputs.dismissBatMission=[missionOwner](const std::string& id){missionOwner(id)->dismissMission(id);};
  commandInputs.batDefinitions=[this]{return bats->batDefinitions();};
  commandInputs.reloadBatDefinitions=[this]{bats->reloadDefinitions();};
  commandInputs.openBatConversation=[this](const std::string& value){
    if(!shell)return;
    try{
      const auto resolved=bats->resolveMissionId(value);
      if(!resolved){statusText="No agent found: "+value;++view.revision;shell->requestRedraw();return;}
      shell->openBatConversation(*resolved);
    }catch(const std::exception& error){
      statusText=error.what();++view.revision;shell->requestRedraw();
    }
  };
  commandInputs.backgroundBatMission=[bats=bats](const std::string& id){
    return bats->backgroundForegroundMission(id);};
  commandInputs.getRuntime=[this]{return runtime;};
  commandInputs.getShell=[this]{return shell;};
  commandInputs.openRewind=[this]{
    if(!frontend)throw std::logic_error("Rewind frontend is unavailable");
    frontend->openRewind();};
  commandInputs.updateWatchPaths=[this](std::vector<std::string> paths){
    if(fileWatcher)fileWatcher->updateWatchPaths(std::move(paths));};
  commandInputs.log=[this](const std::string& value){log(value);};
  commandInputs.onError=[this](std::exception_ptr error){report(error);};
  commandInputs.requestExit=[this]{
    // Let the local command's Promise continuations publish its final messages
    // before the normal shutdown path closes input dispatch.
    owner.postMission([this]{requestExit();});
  };
  return application_commands::Bundle::create(std::move(commandInputs));
}
interactive::context::ContextBindings Host::Impl::makeContextBindings(){
  interactive::context::ContextBindings context;
  context.store=store;
  context.legacySetAppState=[bats=bats](
      const std::function<Json(const Json&)>& update){
    bats->setAppState(update);
  };
  context.loadedNestedMemoryPaths=loadedMemoryPaths;
  context.discoveredSkillNames=discoveredSkills;
  context.contentReplacementState=replacements;
  if(!sdkThinkingConfig)sdkThinkingConfig=std::make_shared<Json>(config.thinking?
    Json{{"type","enabled"},{"budgetTokens",16384}}:
    Json{{"type","disabled"}});
  context.thinkingConfig=sdkThinkingConfig;
  if(!sdkThinkingOverride)sdkThinkingOverride=std::make_shared<bool>(false);
  context.explicitThinkingOverride=sdkThinkingOverride;
  context.initialMcpClients=mcpClientRows?mcpClientRows:
    std::make_shared<Json>(Json::array());
  Json commandRows=Json::array();
  for(const auto& command:commands->entries())
    commandRows.push_back(command.descriptor);
  if(!this->commandRows)this->commandRows=std::make_shared<Json>(Json::array());
  *this->commandRows=std::move(commandRows);
  context.commands=this->commandRows;
  context.catalogSettings.getDenyRuleForGadget=[permission=permission](
      const gadgets::Gadget& gadget,const Json& rules){
    return permission->deniedByRule(gadget.name,"",rules);};
  context.catalogSettings.localeCompare=[](const std::string& a,
      const std::string& b){return a.compare(b);};
  context.assembleGadgets=[roster=roster,settings=context.catalogSettings](
      const Json& permissions,const gadgets::Gadgets&){
    return roster->selectCurrent(permissions,settings).gadgets;};
  context.getMcpGadgets=[roster=roster](const app_state::AppStateRef&){
    const auto snapshot=roster->current();
    return snapshot?snapshot->mcp:gadgets::Gadgets{};};
  context.gadgetSnapshotOwner=[roster=roster]{
    return std::static_pointer_cast<const void>(roster->current());};
  context.gadgetPool.localeCompare=context.catalogSettings.localeCompare;
  context.gadgetPool.feature=[](const std::string&){return false;};
  context.gadgetPool.isCoordinatorMode=[] {return false;};
  context.disabled=true;
  context.onChangeAPIKey=[this]{log("API key change requires restart");};
  context.addNotification=[this](const Json& value){log(value.dump());};
  context.sendOSNotification=[](const Json&){};
  context.onChangeDynamicMcpConfig=[this](const Json&){
    log("Dynamic MCP changes require restart");};
  context.onInstallIDEExtension=[](const std::string&){};
  context.resume=[restore=restore](const std::string& id,const Json& log,
      const std::string& entrypoint){
    return restore->resume(id,log,entrypoint);};
  context.setInProgressGadgetUseIDs=[this](const auto& update){
    inProgressGadgetUseIds=update(inProgressGadgetUseIds);
    if(shell)shell->requestRedraw();};
  context.setHasInterruptibleGadgetInProgress=[this](bool){
    if(shell)shell->requestRedraw();};
  context.spinner.setColor=[](std::optional<std::string>){};
  context.spinner.setShimmerColor=[](std::optional<std::string>){};
  context.spinner.setMessage=[this](std::optional<std::string> value){
    statusText=value.value_or("");if(shell)shell->requestRedraw();};
  return context;
}
void Host::Impl::rebindRoost(
    const application_roost::Snapshot& previous,
    const application_roost::Snapshot& next){
  if(!owner.isOwnerThread()||!runtime||!frontend)
    throw std::logic_error("Workspace rebind needs the live application owner");
  if(!runtime->canReplaceConversation())
    throw std::logic_error("Cannot switch workspaces during a model turn");
  if(previous.cwd.empty()||next.cwd.empty())
    throw std::invalid_argument("Workspace transition requires both directories");
  auto retired=captureRoostGeneration();
  // Roost::activate has already installed next as its live snapshot.
  retired.projectRoot=previous.projectRoot;
  retiredRoosts.push_back(std::move(retired));
  config.cwd=next.cwd;
  lsp.reset();
  roster=std::make_shared<gadgets::live_roster::Roster>();
  bindStorage();
  bindFileHistory();
  bindRoostProviders();
  if(fileWatcher){
    fileWatcher->onCwdChanged(runtime::pathToUtf8(previous.cwd),
      runtime::pathToUtf8(next.cwd)).then(
        [this](const event_loop::Unit*,const std::exception_ptr& error){
          if(error)report(error);});
  }
  bindBackend();
  bindMcpRuntime();
  auto nextCommands=makeCommands();
  commands=nextCommands;
  if(mcpElicitation)mcpElicitation->attachUi(commands->router()->ui());
  auto context=makeContextBindings();
  const auto mcpClients=*context.initialMcpClients;
  frontend->rebindEnvironment(config,poetry,nextCommands->entries(),
    mcpClients,fileHistory);
  application_runtime::EnvironmentRebind replacement;
  replacement.config=config;
  replacement.engine=engine->services();
  replacement.notifyLifecycle=[this](const std::string& id,
      const std::string& state){log("Command "+id+": "+state);};
  replacement.commands=nextCommands->entries();
  replacement.context=std::move(context);
  replacement.onCommandsInstalled=[this,nextCommands]{
    if(shell){
      shell->replaceCommandUi(nextCommands->router()->ui());
      shell->requestRedraw();
    }
  };
  runtime->rebindEnvironment(std::move(replacement));
  mcpRuntime->start().then([this](const event_loop::Unit*,
      const std::exception_ptr& error){
    if(error)log("MCP start failed: "+std::string("connection manager"));
  });
  currentBat.reset();
  bats::saveStashSafeParams({});
  inProgressGadgetUseIds=std::make_shared<runtime::OrderedStringSet>();
  if(completion)completion->clearPathStash();
  releaseRetiredProviders();
  log("Workspace providers rebound: "+next.cwd.string());
}
void Host::Impl::bindFrontend(){
  application_runtime::Inputs bindings(owner,view);
  bindings.config=config;bindings.store=store;
  bindings.initialMessages=std::make_shared<interactive::TurnMessages>();
  bindings.transcriptPersistence.flightId=[flight=flight]{return flight->id();};
  bindings.transcriptPersistence.record=[flight=flight](
      const std::vector<interactive::MessageRef>& slice,
      std::optional<std::string> parent,
      const std::vector<interactive::MessageRef>& all){
    return flight->project()->recordTranscriptRefs(slice,Json::object(),
      std::move(parent),all);
  };
  bindings.transcriptPersistence.lastChainParticipant=[flight=flight](
      const std::vector<interactive::MessageRef>& slice,
      const std::vector<interactive::MessageRef>& all)
      ->std::optional<std::string>{
    const auto cleaned=flights::cleanMessagesForLoggingRefs(slice,all,
      flight->project()->imports().cleaning);
    for(auto item=cleaned.rbegin();item!=cleaned.rend();++item){
      const auto message=runtime::snapshotMessage(*item);
      if(flights::isChainParticipant(message))
        return message.at("uuid").get<std::string>();
    }
    return {};
  };
  bindings.readFileState=fileStash;
  bindings.engine=engine->services();
  bindings.notifyLifecycle=[this](const std::string& id,const std::string& state){
    log("Command "+id+": "+state);};
  bindings.presentationImports.fullscreen=config.fullscreen;
  bindings.presentationImports.stream.connectorTextEnabled=[] {return false;};
  bindings.presentationImports.stream.isConnectorTextBlock=[](const Json& value){
    return value::property(value,"type")=="connector_text";};
  bindings.presentationImports.stream.nowMs=[] {
    return static_cast<double>(platform::nowMillis());};
  bindings.presentationImports.newConversationId=platform::randomUUID;
  bindings.presentationImports.setContextBlocked=[this](bool blocked){
    view.contextBlocked=blocked;};
  bindings.presentationImports.removeTranscriptMessage=[this](const Json& value){
    const auto found=std::find_if(view.messages.begin(),view.messages.end(),
      [&value](const auto& item){return item&&*item==value;});
    if(found!=view.messages.end())view.messages.erase(found);
  };
  application_flights::RestoreInputs restoreInputs(owner);
  restoreInputs.config=config;restoreInputs.flight=flight;
  restoreInputs.roost=roost;
  restoreInputs.rebindRoost=[this](const auto& previous,const auto& next){
    rebindRoost(previous,next);};
  restoreInputs.store=store;restoreInputs.fileStash=fileStash;
  restoreInputs.messages=messages;restoreInputs.replacementState=replacements;
  restoreInputs.setMessages=[this](const Json& values){
    if(!runtime)throw std::logic_error("Resume requires live runtime");
    auto restored=std::make_shared<interactive::TurnMessages>();
    if(values.is_array())for(const auto& value:values)
      restored->push_back(std::make_shared<Json>(value));
    runtime->messages().setMessages(std::move(restored));
    if(shell)shell->messagesChanged();};
  restoreInputs.setConversationId=[this](const std::string& id){
    view.conversationId=id;++view.revision;
    if(shell)shell->requestRedraw();};
  restoreInputs.setReplacementState=[this](
      std::shared_ptr<gadget_results::ContentReplacementState> state){
    if(!state)throw std::invalid_argument("Resume replacement state is empty");
    if(replacements!=state)*replacements=*state;};
  restoreInputs.endFlight=[this](const std::string&){
    event_loop::Deferred<event_loop::Unit> result(owner);
    runtime->awaitTranscriptIdle().then([this,result](const event_loop::Unit*,
        const std::exception_ptr& persistenceError){
      if(persistenceError){result.reject(persistenceError);return;}
      hooks->run("SessionEnd",Json{{"reason","resume"}},"resume").then(
        [result](const std::vector<Json>*,const std::exception_ptr& error){
          if(error)result.reject(error);else result.resolve({});});
    });
    return result;};
  restoreInputs.startFlight=[this](const std::string& id){
    event_loop::Deferred<Json> result(owner);
    hookFlightOverride=id;
    try{
      hooks->run("SessionStart",Json{{"source","resume"}},"resume").then(
        [this,result](const std::vector<Json>* events,
            const std::exception_ptr& error){
          hookFlightOverride.reset();
          if(error){result.reject(error);return;}
          auto collected=hooks->collectFlightStart(events?*events:
            std::vector<Json>{});
          if(cli&&collected.initialUserMessage)
            cli->prependUserMessage(*collected.initialUserMessage);
          if(fileWatcher&&!collected.watchPaths.empty())
            fileWatcher->updateWatchPaths(collected.watchPaths);
          result.resolve(std::move(collected.messages));});
    }catch(...){hookFlightOverride.reset();result.reject(std::current_exception());}
    return result;};
  restoreInputs.canResume=[this]{
    return !runtime||runtime->canReplaceConversation();};
  restoreInputs.fileHistoryEnabled=[this]{
    return fileHistory&&fileHistory->enabled();};
  restoreInputs.onFileHistoryRestored=[this](const Json& snapshots){
    if(fileHistory)fileHistory->restoreStateFromLog(snapshots);};
  restoreInputs.writeOutput=[this](const std::string& value){
    statusText=value;++view.revision;if(shell)shell->requestRedraw();};
  restoreInputs.logError=[this](std::exception_ptr error){report(error);};
  restoreInputs.transitionPermissionMode=[this](
      const std::string& from,const std::string& to){
    if(!flightTransitions)return;
    bootstrap::handlePlanModeTransition(*flightTransitions,from,to);
    bootstrap::handleAutoModeTransition(*flightTransitions,from,to);
    if(from=="plan"&&to!="plan")bootstrap::setHasExitedPlanMode(*flightTransitions,true);
  };
  restore=application_flights::Restore::create(std::move(restoreInputs));
  commands=makeCommands();
  if(mcpElicitation)mcpElicitation->attachUi(commands->router()->ui());
  application_completion::Inputs completionInputs(owner);
  completionInputs.cwd=[roost=roost]{return roost->cwd().string();};
  completionInputs.commands=[this]{
    Json rows=Json::array();
    if(commands)for(const auto& item:commands->entries())
      rows.push_back(item.descriptor);
    return rows;
  };
  completionInputs.log=[this](const std::string& value){log(value);};
  completion=application_completion::Provider::create(
    std::move(completionInputs));
  bindings.commands=commands->entries();
  bindings.context=makeContextBindings();
  auto& context=bindings.context;
  inProgressGadgetUseIds=std::make_shared<runtime::OrderedStringSet>();

  application_frontend::Inputs frontendInputs;
  frontendInputs.messages=messages.get();
  frontendInputs.poetry=poetry;frontendInputs.flight=flight;
  frontendInputs.permission=permission;
  frontendInputs.media=media;frontendInputs.history=history;
  frontendInputs.fileHistory=fileHistory;
  frontendInputs.attachments=[this](const std::string& text,
      const Json& selection,const Json& images,const Json& all,
      const std::optional<std::string>& source){
    if(!attachments)throw std::logic_error("Attachment provider is unavailable");
    return attachments->collectPoetry(text,selection,images,all,source);};
  frontendInputs.userPoetryHooks=[this](const std::string& text,
      const std::string& permissionMode,std::stop_token signal){
    if(!hooks)throw std::logic_error("User prompt hooks are unavailable");
    return hooks->userPoetrySubmit(text,permissionMode,signal);};
  frontendInputs.persistOnTurnComplete=false;
  frontendInputs.feature=[](const std::string&){return false;};
  frontendInputs.log=[this](const std::string& value){log(value);};
  frontendInputs.showImmediateNotice=[this](const std::string& value){
    statusText=value;++view.revision;
    if(shell)shell->requestRedraw();};
  frontendInputs.onError=[this](std::exception_ptr error){report(error);};
  frontendInputs.redraw=[this]{if(shell)shell->requestRedraw();};
  frontendInputs.shutdown=[this]{requestExit();};
  frontendInputs.getRuntime=[this]{return runtime;};
  frontendInputs.getShell=[this]{return shell;};
  frontendInputs.activeAbort=[this]{
    return runtime?runtime->activeAbort():std::shared_ptr<interactive::TurnAbort>{};};
  frontendInputs.hasInterruptibleGadgetInProgress=[this]{
    return inProgressGadgetUseIds&&inProgressGadgetUseIds->size()>0;};
  auto titleFailures=std::make_shared<std::pair<std::string,int>>();
  if(config.autoTitle&&!config.print)frontendInputs.generateFlightTitle=
      [this,titleFailures](const std::string& text){
    event_loop::Deferred<std::optional<std::string>> output(owner);
    try{
      const auto id=flight?flight->id():std::string();
      const bool titled=flight&&(flight->project()->state().currentFlightTitle||
        flight->project()->state().currentFlightAiTitle);
      // Two failed attempts for the same Flight stop further side queries.
      const bool exhausted=titleFailures->first==id&&titleFailures->second>=2;
      if(!model||id.empty()||titled||exhausted){
        output.resolve(std::nullopt);return output;}
      auto generate=application_commands::makeFlightTitleGenerator(owner,model,[this]{
          const auto chosen=value::property(appState(),"mainLoopModel");
          return chosen.is_string()&&!chosen.get<std::string>().empty()?
            chosen.get<std::string>():config.model;
        },false,[this](const std::string& value){log(value);});
      generate(text).then([this,output,id,titleFailures](
          const std::optional<std::string>* title,const std::exception_ptr& error){
        if(error||!title||!*title){
          if(titleFailures->first!=id)*titleFailures={id,0};
          ++titleFailures->second;
          output.resolve(std::nullopt);return;
        }
        try{
          // The Flight may have been cleared or resumed while the query ran.
          if(flight&&flight->id()==id&&!flight->project()->state().currentFlightTitle)
            application_commands::saveFlightMetadataTitle(*flight,"ai-title",**title).then(
              [this](const event_loop::Unit*,const std::exception_ptr& saveError){
                if(saveError)log("Flight title could not be saved");});
        }catch(...){log("Flight title could not be saved");}
        output.resolve(*title);
      });
    }catch(...){log("Flight title generation unavailable");output.resolve(std::nullopt);}
    return output;
  };
  frontendInputs.onQueryContextCreated=[this](
      const interactive::InteractiveQueryContext& created){
    currentBat=created.batContext;};
  application_frontend::IndigenousBashCommandInputs bashInput(owner,*messages);
  bashInput.storage=storage;
  bashInput.getBashGadget=[this]{
    for(const auto* gadget:currentGadgets())
      if(gadget&&gadget->name=="Bash")return *gadget;
    throw std::runtime_error("Live Bash tool is unavailable");
  };
  bashInput.getContext=[this]{
    gadgets::GadgetCallContext call;
    call.data=Json{{"toolPermissionContext",
      value::property(appState(),"toolPermissionContext")}};
    if(const auto abort=runtime?runtime->activeAbort():nullptr)
      call.stopToken=abort->controller.get_token();
    call.batContext=currentBat;
    call.readFileState=fileStash;
    call.loadedNestedMemoryPaths=loadedMemoryPaths;
    call.discoveredSkillNames=discoveredSkills;
    return call;
  };
  bashInput.showProgress=[this](const std::string& command,
      const std::optional<Json>&){
    statusText="Bash: "+command;
    ++view.revision;if(shell)shell->requestRedraw();
  };
  bashInput.clearProgress=[this]{
    if(statusText.rfind("Bash: ",0)==0)statusText.clear();
    ++view.revision;if(shell)shell->requestRedraw();
  };
  frontendInputs.bashCommand=application_frontend::makeIndigenousBashCommand(
    std::move(bashInput));
  frontendInputs.initialMcpClients=*context.initialMcpClients;
  frontend=application_frontend::Bundle::bind(bindings,std::move(frontendInputs));
  if(config.print){
    bindings.execution.getQuerySource=[]{return std::string("sdk");};
    application_cli::Inputs cliInputs(owner);
    cliInputs.input=config.inputFormat=="stream-json"?
      application_cli::InputFormat::StreamJson:
      application_cli::InputFormat::Text;
    cliInputs.output=config.outputFormat=="stream-json"?
      application_cli::OutputFormat::StreamJson:
      config.outputFormat=="json"?application_cli::OutputFormat::Json:
      application_cli::OutputFormat::Text;
    cliInputs.poetry=config.initialPoetry;
    cliInputs.replayUserMessages=config.replayUserMessages;
    cliInputs.verbose=config.verbose;
    cliInputs.flightId=[flight=flight]{return flight->id();};
    cliInputs.messageExists=[flight=flight](const std::string& uuid){
      return flight->doesMessageExistInFlight(uuid);};
    cliInputs.acceptedUserMessage=[this](const std::string& uuid)
        ->std::optional<Json>{
      if(!runtime)return {};
      const auto current=runtime->messages().messages();
      if(!current)return {};
      for(auto it=current->rbegin();it!=current->rend();++it){
        const auto& message=*it;
        if(!message||value::property(*message,"uuid")!=uuid||
            !application_frontend::selectableUserMessage(message))continue;
        return runtime::snapshotMessage(message);
      }
      return {};
    };
    cliInputs.randomUUID=platform::randomUUID;
    cliInputs.messageServices=messages.get();
    cliInputs.durationApiMs=[model=model]{
      return model->snapshot().totalAPIDuration;};
    cliInputs.totalCostUsd=[model=model]{
      return model->snapshot().accounting.totalCostUSD;};
    cliInputs.modelUsage=[model=model]{
      return model->snapshot().accounting.modelUsage;};
    cliInputs.systemInit=[this]{
      Json gadgetNames=Json::array(),commandNames=Json::array(),servers=Json::array();
      for(const auto* gadget:currentGadgets())if(gadget)
        gadgetNames.push_back(gadget->name);
      if(commands)for(const auto& command:commands->entries()){
        if(value::property(command.descriptor,"userInvocable")==false)continue;
        const auto& name=value::property(command.descriptor,"name");
        if(name.is_string())commandNames.push_back(name);
      }
      if(mcpRuntime)for(const auto& client:mcpRuntime->manager()->clients())
        servers.push_back(Json{{"name",value::property(client,"name")},
          {"status",value::property(client,"type")}});
      const auto state=appState();
      const auto& chosen=value::property(state,"mainLoopModel");
      Json batNames=Json::array(),skillNames=Json::array();
      const auto& bats=value::property(value::property(state,"agentDefinitions"),
        "activeAgents");
      if(bats.is_array())for(const auto& bat:bats){
        const auto& type=value::property(bat,"agentType");
        if(type.is_string())batNames.push_back(type);
      }
      if(discoveredSkills)for(const auto& name:*discoveredSkills)
        skillNames.push_back(name);
      const auto& outputStyle=value::property(value::property(state,"settings"),
        "outputStyle");
      return Json{{"cwd",roost->cwd().string()},
        {"tools",gadgetNames},{"slash_commands",commandNames},
        {"mcp_servers",servers},{"agents",batNames},{"skills",skillNames},
        {"model",chosen.is_string()?chosen:Json(config.model)},
        {"permissionMode",mode(state)},
        {"output_style",outputStyle.is_string()?outputStyle:Json("default")}};
    };
    cliInputs.submitPoetry=[this](const std::string& poetry,
        const std::optional<std::string>& uuid){
      if(!runtime||!runtime->runtime())return false;
      Json queued={{"value",poetry},{"mode","prompt"},
        {"querySource","sdk"}};
      if(uuid)queued["uuid"]=*uuid;
      runtime->queue().enqueue(queued);
      return true;};
    cliInputs.interruptTurn=[this]{
      if(const auto abort=runtime?runtime->activeAbort():nullptr)
        abort->abort("interrupt");};
    cliInputs.setModel=[this](const std::string& requested){
      if(!runtime)throw std::logic_error("Model switch needs a live runtime");
      const auto chosen=requested=="default"?config.model:requested;
      if(requested!="default"&&!model->config().profiles.contains(chosen))
        throw std::invalid_argument("Model '"+chosen+"' is not configured");
      store->setState([chosen](const app_state::AppStateRef& previous){
        auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
        next->fields->set("mainLoopModel",chosen);
        next->fields->set("mainLoopModelForSession",nullptr);
        return next;});
      Json breadcrumbs=Json::array();
      breadcrumbs.push_back(user_input::createSyntheticUserCaveatMessage(
        *messages));
      breadcrumbs.push_back(messages::createUserMessage(Json{{"content",
        user_input::formatCommandInputTags("model",requested)}},*messages));
      breadcrumbs.push_back(messages::createUserMessage(Json{{"content",
        "<local-command-stdout>Set model to "+chosen+
          "</local-command-stdout>"}},*messages));
      runtime->messages().setMessages([breadcrumbs](
          const interactive::TurnMessagesRef& previous){
        auto next=std::make_shared<interactive::TurnMessages>(*previous);
        for(const auto& item:breadcrumbs)next->push_back(
          std::make_shared<Json>(item));
        return next;});
      return breadcrumbs;};
    cliInputs.mcpStatus=[this]{
      Json statuses=Json::array();
      if(!mcpRuntime)return statuses;
      for(const auto& server:mcpRuntime->manager()->snapshot()){
        Json config=Json::object();
        const auto& kind=value::property(server.config,"type");
        const auto& url=value::property(server.config,"url");
        if(kind=="http"||kind=="sse"||
            (kind.is_null()&&url.is_string())){
          config["type"]=kind.is_null()?Json("http"):kind;
          for(const auto* key:{"url","headers","oauth"})
            if(server.config.contains(key))config[key]=server.config.at(key);
        }else if(kind=="claudeai-proxy"){
          config["type"]=kind;
          for(const auto* key:{"url","id"})
            if(server.config.contains(key))config[key]=server.config.at(key);
        }else if(kind=="stdio"||kind.is_null()){
          config["type"]="stdio";
          for(const auto* key:{"command","args"})
            if(server.config.contains(key))config[key]=server.config.at(key);
        }
        Json status={{"name",server.name},{"status",server.type}};
        if(!config.empty())status["config"]=std::move(config);
        const auto& scope=value::property(server.config,"scope");
        if(!scope.is_null()&&!scope.is_discarded())status["scope"]=scope;
        if(server.type=="failed"&&!server.error.empty())
          status["error"]=server.error;
        if(server.type=="connected"){
          if(server.connection)status["serverInfo"]=server.connection->serverInfo;
          Json gadgets=Json::array();
          for(const auto& gadget:server.gadgets){
            const auto& unprefixed=value::property(gadget.mcpInfo,"toolName");
            Json annotations=Json::object();
            if(gadget.isReadOnly&&gadget.isReadOnly(Json::object()))
              annotations["readOnly"]=true;
            if(gadget.isDestructive&&gadget.isDestructive(Json::object()))
              annotations["destructive"]=true;
            if(gadget.isOpenWorld&&gadget.isOpenWorld(Json::object()))
              annotations["openWorld"]=true;
            gadgets.push_back(Json{{"name",unprefixed.is_string()?unprefixed:
              Json(gadget.name)},{"annotations",std::move(annotations)}});
          }
          status["tools"]=std::move(gadgets);
        }
        statuses.push_back(std::move(status));
      }
      return statuses;};
    cliInputs.manageMcp=[this](const std::string& name,
        std::optional<bool> enabled){
      event_loop::Deferred<Json> result(owner);
      if(!mcpRuntime){
        result.reject(std::make_exception_ptr(std::runtime_error(
          "MCP connection manager is unavailable")));
        return result;
      }
      const auto manager=mcpRuntime->manager();
      const auto current=manager->snapshot();
      const auto found=std::find_if(current.begin(),current.end(),
        [&name](const auto& server){return server.name==name;});
      if(found==current.end()){
        result.reject(std::make_exception_ptr(std::runtime_error(
          "Server not found: "+name)));
        return result;
      }
      const auto status=[manager,name]{
        manager->flushPending();
        const auto rows=manager->snapshot();
        const auto selected=std::find_if(rows.begin(),rows.end(),
          [&name](const auto& server){return server.name==name;});
        if(selected==rows.end())throw std::runtime_error("Server not found: "+name);
        Json value={{"type",selected->type}};
        if(!selected->error.empty())value["error"]=selected->error;
        return value;
      };
      try{
        if(enabled&&!*enabled){
          if(found->type=="disabled")result.resolve(status());
          else manager->toggle(name).then([result,status](const event_loop::Unit*,
              const std::exception_ptr& error){
            if(error)result.reject(error);
            else try{result.resolve(status());}catch(...){result.reject(
              std::current_exception());}
          });
        }else if(enabled&&*enabled&&found->type=="disabled"){
          manager->toggle(name).then([result,status](const event_loop::Unit*,
              const std::exception_ptr& error){
            if(error)result.reject(error);
            else try{result.resolve(status());}catch(...){result.reject(
              std::current_exception());}
          });
        }else manager->reconnect(name).then([result,manager](
            const mcp::manager::ServerResult* server,
            const std::exception_ptr& error){
          if(error)result.reject(error);
          else if(!server)result.reject(std::make_exception_ptr(
            std::runtime_error("MCP reconnect returned no server")));
          else try{
            manager->flushPending();
            Json value={{"type",server->type}};
            if(!server->error.empty())value["error"]=server->error;
            result.resolve(std::move(value));
          }catch(...){result.reject(std::current_exception());}
        });
      }catch(...){result.reject(std::current_exception());}
      return result;
    };
    cliInputs.setMaxThinkingTokens=[this](const Json& value){
      if(!sdkThinkingConfig||!sdkThinkingOverride)
        throw std::logic_error("Thinking configuration is unavailable");
      if(value.is_null()){
        *sdkThinkingConfig=config.thinking?
          Json{{"type","enabled"},{"budgetTokens",16384}}:
          Json{{"type","disabled"}};
        *sdkThinkingOverride=false;
      }else{
        *sdkThinkingConfig=value==0?Json{{"type","disabled"}}:
          Json{{"type","enabled"},{"budgetTokens",value}};
        *sdkThinkingOverride=true;
      }
    };
    cliInputs.setPermissionMode=[this](const Json& request,
        const std::string& requestId){
      const auto& requested=value::property(request,"mode");
      if(!requested.is_string())
        throw std::invalid_argument("Permission mode must be a string");
      const auto nextMode=requested.get<std::string>();
      if(nextMode!="default"&&nextMode!="acceptEdits"&&
          nextMode!="bypassPermissions"&&nextMode!="plan"&&
          nextMode!="dontAsk")
        throw std::invalid_argument("Unknown permission mode: "+nextMode);
      const auto current=value::property(appState(),"toolPermissionContext");
      cli::headless::PermissionControlServices services;
      services.isBypassPermissionsModeDisabled=[this]{
        return bypassPermissionsDisabled(config,value::property(appState(),"settings"));};
      services.transcriptClassifierEnabled=[] {return false;};
      services.isAutoModeGateEnabled=[] {return false;};
      services.getAutoModeUnavailableReason=[] {return std::string{};};
      services.getAutoModeUnavailableNotification=[](const std::string& reason){
        return reason;};
      services.transitionPermissionMode=[transitions=flightTransitions](
          const std::string& from,const std::string& to,const Json& context){
        if(from==to)return context;
        if(transitions){
          bootstrap::handlePlanModeTransition(*transitions,from,to);
          bootstrap::handleAutoModeTransition(*transitions,from,to);
          if(from=="plan"&&to!="plan")
            bootstrap::setHasExitedPlanMode(*transitions,true);
        }
        Json next=context;
        if(from=="plan"&&to!="plan"&&next.is_object())
          next.erase("prePlanMode");
        return next;};
      Json reply;
      services.enqueue=[&reply](const Json& value){reply=value;};
      const auto next=cli::headless::handleSetPermissionMode(request,
        requestId,current,services);
      const auto& ultraplan=value::property(request,"ultraplan");
      const bool accepted=value::property(value::property(reply,"response"),
        "subtype")=="success";
      if(accepted||ultraplan.is_boolean()){
        store->setState([next,ultraplan,accepted](
            const app_state::AppStateRef& previous){
          auto updated=std::make_shared<app_state::AppState>(previous->shallowCopy());
          if(accepted)updated->setPermissionContext(
            std::make_shared<value::SharedRecord>(
              value::SharedRecord::fromJson(next)));
          if(ultraplan.is_boolean())
            updated->fields->set("isUltraplanMode",ultraplan);
          return updated;});
      }
      return reply;};
    cliInputs.requestExit=[this](bool lastResultError){
      if(lastResultError)failed.store(true);
      requestExit();};
    cliInputs.report=[this](std::exception_ptr error){report(error);};
    cli=application_cli::Adapter::create(std::move(cliInputs));
    bindings.onTurnRequested=[this](const interactive::TurnRequest& request){
      cli->onTurnRequested(request);
    };
    const auto previousEvent=bindings.execution.onQueryEvent;
    bindings.execution.onQueryEvent=[this,previousEvent](
        const query::AsyncEngineStep& step){
      if(previousEvent)previousEvent(step);
      cli->onQueryEvent(step);};
    auto original=bindings.turn.mrOnTurnComplete;
    bindings.turn.mrOnTurnComplete=[this,original](
        const interactive::TurnMessagesRef& turn,bool aborted){
      auto result=original(turn,aborted);
      result.then([this,turn,aborted](const event_loop::Unit*,
          const std::exception_ptr& error){
        cli->onTurnComplete(turn,aborted,error);});
      return result;};
  }
  const auto completed=bindings.turn.mrOnTurnComplete;
  bindings.turn.mrOnTurnComplete=[this,completed](
      const interactive::TurnMessagesRef& turn,bool aborted){
    auto result=completed(turn,aborted);
    result.then([this](const event_loop::Unit*,const std::exception_ptr&){
      owner.queueNextTurn([this]{releaseRetiredProviders();});
    });return result;
  };
  runtime=application_runtime::ApplicationRuntime::create(std::move(bindings));

  terminal_ftxui::Options options;
  options.mode=config.fullscreen?terminal_ftxui::Mode::Fullscreen:
    terminal_ftxui::Mode::Inline;
  options.interactive=!config.print;
  interactive::conversation_shell::Imports shellInputs(owner,view,options);
  runtime->bindShellImports(shellInputs);
  shellInputs.input.presentation.fullscreen=config.fullscreen;
  shellInputs.input.presentation.stream.connectorTextEnabled=[] {return false;};
  shellInputs.input.presentation.stream.isConnectorTextBlock=[](const Json& value){
    return value::property(value,"type")=="connector_text";};
  shellInputs.input.presentation.stream.nowMs=[] {
    return static_cast<double>(platform::nowMillis());};
  shellInputs.input.presentation.newConversationId=platform::randomUUID;
  shellInputs.input.presentation.setContextBlocked=[this](bool blocked){
    view.contextBlocked=blocked;};
  shellInputs.input.presentation.removeTranscriptMessage=[](const Json&){};
  shellInputs.input.onTerminalResponse=[](termio::ResponseRef){};
  shellInputs.input.onMouse=[](keyboard_input::MouseRef){};
  shellInputs.input.onTerminalFocus=[this](bool focused){
    view.input.focused=focused;};
#ifdef _WIN32
  shellInputs.input.onSuspend=[] {};
#else
  shellInputs.input.onSuspend=[] {std::raise(SIGTSTP);};
#endif
  shellInputs.input.onQueueBlocked=[](bool){};
  shellInputs.input.onCommandSettled=[this](const interactive::CommandDescriptor&,
      const interactive::CommandCompletion&,bool){releaseRetiredProviders();};
  shellInputs.input.onCommandError=[this](const std::string& value){log(value);};
  shellInputs.input.writePlain=[this](std::string_view value){
    if(config.print)return;
    std::cout.write(value.data(),static_cast<std::streamsize>(value.size()));
    std::cout.flush();};
  shellInputs.input.onExit=[this]{requestExit();};
  shellInputs.backgroundForegroundMissions=[this]{
    return commands&&commands->backgroundForegroundMissions();};
  shellInputs.missionSnapshot=[this]{
    Json missions=value::property(appState(),"tasks");
    if(!missions.is_object())missions=Json::object();
    if(managedShell.missions)for(const auto& mission:managedShell.missions->list()){
      if(mission.type!="local_bash")continue;
      missions[mission.id]={{"type",mission.type},{"status",mission.status},
        {"description",mission.description},{"isBackgrounded",mission.isBackgrounded}};
    }
    return missions;
  };
  shellInputs.openBackgroundMission=[this](const std::string& id){
    if(!commands)return;
    const auto state=appState();
    if(value::property(value::property(value::property(state,"tasks"),id),"type")=="dream")
      (void)commands->openUiCommand("memory");
    else commands->openMissions(id);
  };
  shellInputs.openControlCommand=[this](const std::string& name,const std::string& args){
    return commands&&commands->openUiCommand(name,args);};
  shellInputs.observeBatMission=[this](const std::string& id){
    return observeBatMission(id);};
  shellInputs.batMissionSnapshot=[this](const std::string& id)->std::optional<Json>{
    const auto state=appState();const auto& mission=value::property(value::property(state,"tasks"),id);
    return mission.is_object()?std::optional(mission):std::nullopt;
  };
  shellInputs.submitBatMessage=[this](const std::string& id,const std::string& text){
    return submitBatMessage(id,text);};
  shellInputs.releaseBatMission=[this](const std::string& id){
    batOwner(id)->releaseMission(id);
    owner.queueNextTurn([this]{releaseRetiredProviders();});};
  shellInputs.stopBatMission=[this](const std::string& id){batOwner(id)->stopMission(id);};
  shellInputs.onRepin=[this]{view.viewport.follow=true;};
  shellInputs.getSubmitCount=[] {return std::size_t{};};
  shellInputs.copyMessageText=[this](const std::string& text){
    media->copyMessageText(text,[this](const std::string& raw){
      if(shell)shell->driver().writeRaw(raw);},
      [this](const interactive::navigation::CopyNotification& notice){
        statusText=notice.text;++view.revision;
        if(shell)shell->requestRedraw();});};
  shellInputs.editMessage=[this](interactive::MessageRef message){
    return frontend->editMessage(std::move(message));};
  shellInputs.getImageFromClipboard=[media=media]{
    return media->getImageFromClipboard();};
  shellInputs.onImagePasted=[media=media](const Json& image){
    media->acceptPastedImage(image);};
  shellInputs.readPastedImagePaths=[media=media](const std::string& value){
    return media->extractPastedImages(value);};
  shellInputs.history=history;
  shellInputs.completion=completion;
  shellInputs.nowMs=[] {return static_cast<double>(platform::nowMillis());};
  shellInputs.logForDebugging=[this](const std::string& value){log(value);};
  shellInputs.terminalProgressEnabled=[] {return false;};
  shellInputs.setTerminalProgress=[](std::optional<interactive::message_list_view::ProgressState>){};
  shellInputs.onDetachedError=[this](std::exception_ptr error){report(error);};
  shellInputs.editorCallbacks.invert=[](const std::string& value){return value;};
  shellInputs.editorCallbacks.onChange=[this](const std::string& text){
    view.input.text=text;};
  shellInputs.editorCallbacks.onOffsetChange=[this](std::size_t value){
    view.input.caretUtf16=value;};
  shellInputs.editorCallbacks.addNotification=[this](
      const interactive::text_input::NotificationRequest& notification){
    editorNotification={{notification.key,notification.text}};
    statusText=notification.text;++view.revision;
    if(shell)shell->requestRedraw();
  };
  shellInputs.editorCallbacks.removeNotification=[this](const std::string& key){
    if(!editorNotification||editorNotification->first!=key)return;
    if(statusText==editorNotification->second){
      statusText.clear();++view.revision;
      if(shell)shell->requestRedraw();
    }
    editorNotification.reset();
  };
  shellInputs.editorOptions.fullscreen=config.fullscreen;
  const auto terminal=indigenousEnvironment("TERM");
  const bool colors=indigenousEnvironment("NO_COLOR").empty()&&terminal!="dumb";
  const auto colorTerm=indigenousEnvironment("COLORTERM");
  const bool richColors=colorTerm=="truecolor"||colorTerm=="24bit"||
    terminal.find("direct")!=std::string::npos||terminal=="xterm-kitty";
  const std::string indigenousTheme=!colors?"monochrome":richColors?"halloween":"halloween-ansi";
  theme=std::make_shared<interactive::presentation_providers::ThemeState>(
    interactive::presentation_providers::ThemeStateServices{
      [indigenousTheme] {return indigenousTheme;},[](const std::string&){},
      [] {return std::string("dark");},[this]{if(shell)shell->requestRedraw();}});
  application_presentation::Inputs presentInputs;
  presentInputs.theme=theme.get();
  presentInputs.shortcuts=interactive::presentation_providers::makeShortcutProvider(
#ifdef __APPLE__
    {"macos",true,[](const std::string&){return false;}},
#elif defined(_WIN32)
    {"win32",true,[](const std::string&){return false;}},
#else
    {"linux",true,[](const std::string&){return false;}},
#endif
    [] {return std::vector<interactive::presentation_providers::Binding>{};});
  presentInputs.messageServices=messages.get();
  presentInputs.colors=colors;
  presentInputs.hyperlinks=colors&&terminal!="dumb"&&
    (terminal.find("xterm")!=std::string::npos||!indigenousEnvironment("TERM_PROGRAM").empty());
  presentInputs.storedImagePath=[media=media](int id){return media->storedImagePath(id);};
  presentInputs.gadgets=[this]{return currentGadgets();};
  presentInputs.commands=[this]{
    Json result=Json::array();
    if(commands)for(const auto& item:commands->entries())
      result.push_back(item.descriptor);
    return result;
  };
  presentInputs.modelLabel=[this]{
    const auto& state=appState();
    for(const auto* key:{"mainLoopModelForSession","mainLoopModel"}){
      const auto& selected=value::property(state,key);
      if(selected.is_string()&&!selected.get<std::string>().empty())
        return selected.get<std::string>();
    }
    return config.model;
  };
  presentInputs.workingDirectory=[roost=roost]{
    return roost->cwd().string();};
  presentInputs.permissionMode=[this]{return mode(appState());};
  presentInputs.statusText=[this]{return statusText;};
  presentInputs.feature=[](const std::string&){return false;};
  presentInputs.log=[this](const std::string& value){log(value);};
  presentation=application_presentation::Bundle::bind(
    shellInputs,std::move(presentInputs));
  shell=interactive::conversation_shell::Shell::create(std::move(shellInputs));
  shell->attachCommandUi(commands->router()->ui());
  runtime->attachShell(shell);
  permission->attachUi(shell->commands().shared_from_this());
}
}
