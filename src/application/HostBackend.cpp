#include "application/HostInternal.hpp"
#include "application/ModelBinding.hpp"
#include "application_engine/CompactBinding.hpp"
#include "application_engine/Defaults.hpp"
#include "application_engine/Execution.hpp"
#include "application_gadgets/DefaultInputs.hpp"
#include "application_commands/CommandsMcp.hpp"
#include "mcp/OutputStorage.hpp"
#include "application/Async.hpp"
#include "runtime/Platform.hpp"
#include <stdexcept>

namespace batbox::application {
namespace {
gadgets::Gadgets snapshotGadgets(
    const std::shared_ptr<gadgets::live_roster::Roster>& roster){
  const auto snapshot=roster->current();
  if(!snapshot)return {};
  gadgets::Gadgets all=snapshot->base;
  all.insert(all.end(),snapshot->mcp.begin(),snapshot->mcp.end());
  return all;
}
void replaceJsonState(const std::shared_ptr<app_state::AppStateStore>& store,
    const std::function<Json(const Json&)>& update){
  store->setState([update](const app_state::AppStateRef& previous){
    const Json before=previous->snapshot();const Json after=update(before);
    if(!after.is_object())throw std::invalid_argument("App state update must return an object");
    if(after==before)return previous;
    auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
    for(auto it=after.begin();it!=after.end();++it)next->fields->set(it.key(),it.value());
    const auto oldPermission=value::property(before,"toolPermissionContext");
    const auto newPermission=value::property(after,"toolPermissionContext");
    if(newPermission.is_object()&&newPermission!=oldPermission)
      next->setPermissionContext(std::make_shared<value::SharedRecord>(
        value::SharedRecord::fromJson(newPermission)));
    return next;
  });
}
}
void Host::Impl::bindBackend(){
  mcpClientRows=std::make_shared<Json>(Json::array());
  commandRows=std::make_shared<Json>(Json::array());
  mcpRuntimeSlot=std::make_shared<std::weak_ptr<mcp::manager::IndigenousRuntime>>();
  auto generationBats=generationBatSlot;
  lsp=application_lsp::Runtime::create(bindingProjectRoot(),
    config.settings.value("lsp",config.settings));
  application_gadgets::DefaultInputs gadgetDefaults{owner};
  gadgetDefaults.config=config;gadgetDefaults.store=store;
  gadgetDefaults.messages=messages;gadgetDefaults.fileStash=fileStash;
  gadgetDefaults.permission=permission;gadgetDefaults.connectors=connectors;
  gadgetDefaults.model=model;gadgetDefaults.shell=managedShell;
  gadgetDefaults.fileHistory=fileHistory;gadgetDefaults.lsp=lsp;
  gadgetDefaults.storage=storage;
  gadgetDefaults.missionListId=[flight=flight]{return flight->id();};
  gadgetDefaults.missionLifecycleHooks=[hooks=hooks,dispatch=owner.dispatcher()](
      const std::string& event,const Json& detail,const gadgets::GadgetCallContext& call){
    execution::Context context=call.batContext?call.batContext->query:
      execution::Context{};
    context.data=call.data;
    const Json input=detail;
    const auto values=awaitOnOwner<std::vector<Json>>(dispatch,
      [hooks,event,input,context,signal=call.stopToken]{
        return hooks->run(event,input,{},signal,context);
      });
    std::vector<std::string> blocking;
    for(const auto& value:values){
      const auto& error=value::property(value,"blockingError");
      if(!value::truthy(error))continue;
      const auto& message=value::property(error,"blockingError");
      if(message.is_string())blocking.push_back(event+" hook feedback:\n"+
        message.get<std::string>());
    }
    return blocking;
  };
  gadgetDefaults.expandMissionList=[dispatch=owner.dispatcher(),store=store]{
    onOwnerAndWait<void>(dispatch,[store]{
      store->setState([](const app_state::AppStateRef& previous){
        auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
        next->fields->set("expandedView","tasks");return next;
      });
    });
  };
  gadgetDefaults.skillRoster=skills->roster();
  gadgetDefaults.skillServices=skills->rosterServices();
  gadgetDefaults.additionalGadgets.push_back(skills->makeGadget());
  gadgetDefaults.makeBatGadget=[generationBats]{
    auto owner=generationBats->lock();
    if(!owner)throw std::logic_error("Native Agent owner is unavailable");
    return owner->makeGadget();
  };
  gadgetDefaults.onError=[this](std::exception_ptr error){report(error);};
  gadgetDefaults.log=[this](const std::string& value){log(value);};
  gadgetDefaults.showBackgroundHint=[this,dispatch=owner.dispatcher()](const Json&){
    dispatch.postMission([this]{
      if(closing)return;
      statusText="Ctrl+B to run in background";
      ++view.revision;
      if(shell)shell->requestRedraw();
    });
  };
  preparedGadgets=application_gadgets::prepare(
    application_gadgets::makeDefaultInputs(std::move(gadgetDefaults)));
  mcpServices=preparedGadgets->factory.mcpServices;
  if(!preparedGadgets->factory.read)
    throw std::logic_error("Read tool service unavailable for compaction");
  auto readServices=std::make_shared<gadgets::files::FileReadServices>(
    *preparedGadgets->factory.read);
  application_media::AggregateInputs attachmentInputs(owner);
  attachmentInputs.files.readServices=[readServices]{return readServices;};
  attachmentInputs.files.getAppState=[store=store]{return store->getState();};
  attachmentInputs.files.isFileReadDenied=[permission=permission](
      const std::string& path,const app_state::AppStateRef& state){
    return permission->deniedByRule("Read",path,state->snapshot());};
  attachmentInputs.files.readFileState=fileStash;
  attachmentInputs.files.messages=messages;
  attachmentInputs.files.currentContext=[weak=std::weak_ptr(roster),store=store,
      stash=fileStash,loaded=loadedMemoryPaths,
      discovered=discoveredSkills,clients=mcpClientRows,
      defaultModel=config.model]{
    const auto generation=weak.lock();
    if(!generation)throw std::runtime_error("Attachment workspace tool catalog has retired");
    execution::Context context;
    context.gadgetSnapshotOwner=generation->current();
    context.gadgets=snapshotGadgets(generation);
    context.baseGadgets=context.gadgets;
    context.readFileState=stash;
    context.loadedNestedMemoryPaths=loaded;
    context.discoveredSkillNames=discovered;
    const auto state=store->getState()->snapshot();
    const auto& selected=value::property(state,"mainLoopModel");
    context.data["options"]={{"mainLoopModel",
      selected.is_string()?selected:Json(defaultModel)},
      {"mcpClients",*clients},
      {"agentDefinitions",value::property(state,"agentDefinitions")}};
    return context;
  };
  attachmentInputs.lspAvailable=[lsp=lsp]{return lsp->available();};
  attachmentInputs.checkLspDiagnostics=[lsp=lsp]{return lsp->takeDiagnosticSets();};
  attachmentInputs.clearLspDiagnostics=[]{};
  attachmentInputs.media=media;
  attachmentInputs.cwd=[cwd=config.cwd]{return cwd.string();};
  attachmentInputs.flightId=[flight=flight]{return flight->id();};
  attachmentInputs.settings=[store=store]{
    return value::property(store->getState()->snapshot(),"settings");};
  attachmentInputs.drainBatPendingMessages=[generationBats](const std::string& id){
    if(auto bat=generationBats->lock())return bat->drainPendingMessages(id);
    return Json::array();
  };
  attachmentInputs.skillCommands=[rows=commandRows]{return *rows;};
  attachmentInputs.mcpClients=[rows=mcpClientRows]{return *rows;};
  attachmentInputs.mcpResources=[slot=mcpRuntimeSlot]{
    if(auto manager=slot->lock())return manager->manager()->resources();
    return Json::object();};
  attachmentInputs.readMcpResource=[slot=mcpRuntimeSlot](
      const std::string& server,const std::string& uri,std::stop_token stop){
    auto promise=std::make_shared<std::promise<Json>>();
    auto future=promise->get_future();
    try{
      auto runtime=slot->lock();
      if(!runtime)throw std::runtime_error("MCP resource manager is unavailable");
      runtime->manager()->invoke(server,"resources/read",Json{{"uri",uri}},stop)
        .then([promise](const Json* value,const std::exception_ptr& error){
          if(error)promise->set_exception(error);
          else if(value)promise->set_value(*value);
          else promise->set_exception(std::make_exception_ptr(
            std::runtime_error("MCP resource read returned no content")));
        });
    }catch(...){promise->set_exception(std::current_exception());}
    return future;
  };
  attachmentInputs.modelSupportsGadgetReference=[](const std::string&){return false;};
  if(!flightTransitions)
    flightTransitions=std::make_shared<bootstrap::FlightTransitions>();
  attachmentInputs.transitions=flightTransitions;
  attachmentInputs.effectiveContextWindow=[model=model](const std::string& id){
    const auto found=model->config().profiles.find(id);
    return found==model->config().profiles.end()?0.0:found->second.contextTokens;};
  attachmentInputs.log=[this](const std::string& value){log(value);};
  attachmentInputs.getAsyncHookResponses=[hooks=hooks]{
    return hooks->asyncHookResponses();};
  attachments=application_media::AggregateProvider::create(
    std::move(attachmentInputs));
  skills->bindRuntime(attachments,hooks);
  auto generationEngine=generationEngineSlot;
  application_engine::CompactBindingInputs compactInputs(owner);
  compactInputs.config=config;compactInputs.model=model;
  compactInputs.messages=messages;compactInputs.hooks=hooks;
  compactInputs.poetry=poetry;compactInputs.flight=flight;
  compactInputs.store=store;compactInputs.permission=permission;
  compactInputs.readServices=[readServices]{return *readServices;};
  compactInputs.normalizeAttachment=makeAttachmentNormalizer(messages,roster);
  compactInputs.invokedSkills=invokedSkills;
  compactInputs.updateWatchPaths=[this](std::vector<std::string> paths){
    if(fileWatcher)fileWatcher->updateWatchPaths(std::move(paths));};
  compactInputs.currentEngine=[generationEngine]{return generationEngine->lock();};
  compactInputs.notifyCommandLifecycle=[this](const std::string& id,
      const std::string& status){log("Command "+id+": "+status);};
  compactInputs.log=[this](const std::string& value){log(value);};
  compactInputs.progress=[this](const Json&){
    if(shell)shell->requestRedraw();};
  compactInputs.notification=[this](const Json& value){
    log("Compaction: "+value.dump());};
  compactor=application_engine::makeCompactor(std::move(compactInputs));

  application_engine::ExecutionInputs executionInputs(owner);
  executionInputs.messages=messages;executionInputs.storage=storage;
  executionInputs.getAppState=[this]{return appState();};
  executionInputs.updateAppState=[this](const auto& update){
    if(bats)bats->setAppState(update);
    else replaceJsonState(store,update);
  };
  executionInputs.currentBatContext=[this]{return currentBat;};
  executionInputs.gadgetProgress=[this](const std::string&,bool){
    if(shell)shell->requestRedraw();};
  executionInputs.gadgetDuration=[](double){};
  executionInputs.imports.startSpeculativeClassifierCheck=[](
      const std::string&,const Json&,const execution::Context&){};
  executionInputs.imports.setHasInterruptibleGadgetInProgress=[this](bool){
    if(shell)shell->requestRedraw();};
  executionInputs.imports.currentAbortReason=[this]{
    return currentBat&&currentBat->currentAbortReason?
      currentBat->currentAbortReason():std::string{};};
  executionInputs.imports.canUseGadget=[this](const Json& input){
    return permission->check("Agent",input,
      value::property(appState(),"toolPermissionContext"));};
  auto execution=application_engine::makeExecutionServices(
    std::move(executionInputs));

  application_memory::Inputs memoryInputs(owner);
  memoryInputs.config=config;memoryInputs.poetry=poetry;memoryInputs.model=model;
  memoryInputs.flight=flight;memoryInputs.messages=messages;
  memoryInputs.getAppState=[store=store]{return store->getState()->snapshot();};
  memoryInputs.updateAppState=[store=store](const auto& update){replaceJsonState(store,update);};
  memoryInputs.activeBatContext=[this]{return currentBat;};
  memoryInputs.currentEngine=[generationEngine]{return generationEngine->lock();};
  memoryInputs.log=[this](const std::string& value){log(value);};
  memoryInputs.notify=[this](const Json& value){
    log("Memory: "+value.dump());if(shell)shell->requestRedraw();};
  memory=application_memory::Provider::create(std::move(memoryInputs));

  application_engine::DefaultInputs engineDefaults(owner);
  auto& engineInputs=engineDefaults.engine;
  engineInputs.config=config;engineInputs.flightId=flightId;
  engineInputs.model=model;engineInputs.messages=messages;
  engineInputs.execution=execution;
  engineInputs.getAppState=[this,dispatch=owner.dispatcher()](
      const execution::Context& context){
    return onOwnerAndWait<Json>(dispatch,[this,context]{
      return execution::effectiveAppState(context,appState());});
  };
  engineInputs.checkpoint=[this](const std::string& value){log(value);};
  engineInputs.logError=[this](const std::exception& error){log(error.what());};
  engineInputs.addNotification=[this](const Json& value){log(value.dump());};
  application_engine::bindCompactor(engineInputs,compactor);
  engineDefaults.poetry=poetry;engineDefaults.flight=flight;
  engineDefaults.hooks=hooks;engineDefaults.permission=permission;
  engineDefaults.store=store;
  engineDefaults.memoryPrefetch=memory->makePrefetchServices();
  engineDefaults.executeAutoDream=[memory=memory](const Json& context){
    memory->executeAutoDream(context);};
  engineDefaults.normalizeAttachment=makeAttachmentNormalizer(messages,roster);
  engineDefaults.activeBatContext=[this]{return currentBat;};
  engineDefaults.currentGadgets=[weak=std::weak_ptr(roster)]{
    const auto generation=weak.lock();
    if(!generation)throw std::runtime_error("Engine workspace tool catalog has retired");
    return snapshotGadgets(generation);};
  engineDefaults.commandQueue=[this]() -> interactive::submit::CommandQueue& {
    if(!runtime)throw std::logic_error("Command queue is not ready");
    return runtime->queue();};
  engineDefaults.getAttachmentMessages=[attachments=attachments](
      const Json& queued,const execution::Context& context,
      const Json& messages,const std::string& source){
    return attachments->turnCursor(queued,context,messages,source);};
  engineDefaults.notifyCommandLifecycle=[this](const std::string& id,
      const std::string& status){log("Command "+id+": "+status);};
  const auto notifyLifecycle=engineDefaults.notifyCommandLifecycle;
  auto configuredEngine=application_engine::makeDefaultEngineInputs(std::move(engineDefaults));
  auto postGadgetUse=configuredEngine.execution->postGadgetUseHooks;
  configuredEngine.execution->postGadgetUseHooks=[postGadgetUse,memory=memory,
      dispatch=owner.dispatcher()](const gadgets::Gadget& gadget,const Json& input,
      const Json& result,const execution::Context& context,
      const std::function<void(const Json&)>& yield){
    if(gadget.name=="Write"||gadget.name=="Edit"||gadget.name=="Bash"){
      const auto& raw=value::property(input,"file_path");
      const auto path=raw.is_string()?raw.get<std::string>():std::string{};
      onOwnerAndWait<void>(dispatch,[memory,path]{memory->observeFileChange(path);});
    }
    if(postGadgetUse)postGadgetUse(gadget,input,result,context,yield);
  };
  engine=application_engine::Engine::create(std::move(configuredEngine));
  *generationEngine=engine;

  application_bats::IndigenousBatApplication::Inputs batInputs(owner);
  batInputs.config=config;batInputs.flightId=flightId;
  batInputs.projectRoot=bindingProjectRoot();
  batInputs.currentFlightId=[flight=flight]{return flight->id();};
  // An idle child reconstructs its conversation from disk. Its context belongs
  // to this roost generation even when the foreground moved elsewhere.
  batInputs.currentBatContext=[this,weak=std::weak_ptr(roster),stash=fileStash,
      loaded=loadedMemoryPaths,discovered=discoveredSkills,replacements=replacements,
      clients=mcpClientRows,commands=commandRows,selected=config]{
    const auto generation=weak.lock();
    if(!generation)throw std::runtime_error("Agent workspace tool catalog has retired");
    if(generation==roster&&currentBat)return *currentBat;
    bats::BatContext context;
    context.query.gadgetSnapshotOwner=generation->current();
    context.query.gadgets=snapshotGadgets(generation);
    context.query.baseGadgets=context.query.gadgets;
    context.query.readFileState=stash;
    context.query.loadedNestedMemoryPaths=loaded;
    context.query.discoveredSkillNames=discovered;
    context.query.contentReplacementState=replacements;
    context.query.data={{"messages",Json::array()},
      {"options",{{"mainLoopModel",selected.model},{"mcpClients",*clients},
        {"commands",*commands},{"verbose",selected.verbose},
        {"thinkingConfig",{{"type",selected.thinking?"enabled":"disabled"}}},
        {"isNonInteractiveSession",selected.print}}}};
    context.getAppState=[store=store]{return store->getState()->snapshot();};
    context.setAppState=[store=store](const auto& update){replaceJsonState(store,update);};
    context.setAppStateForMissions=context.setAppState;
    return context;
  };
  batInputs.currentGadgetOwner=[weak=std::weak_ptr(roster)]{
    const auto generation=weak.lock();
    if(!generation)throw std::runtime_error("Agent workspace tool catalog has retired");
    return std::static_pointer_cast<const void>(generation->current());};
  batInputs.store=store;batInputs.engine=engine;
  batInputs.model=model;batInputs.messages=messages;
  batInputs.permissions=permission;
  batInputs.skills=skills;
  batInputs.hooks=hooks;
  batInputs.batMcpRuntime=[slot=mcpRuntimeSlot]{return slot->lock();};
  batInputs.currentGadgets=[weak=std::weak_ptr(roster)]{
    const auto generation=weak.lock();
    if(!generation)throw std::runtime_error("Agent workspace tool catalog has retired");
    return snapshotGadgets(generation);};
  batInputs.getUserContextAsync=[poetry=poetry]{return poetry->userContext();};
  batInputs.getSystemContextAsync=[poetry=poetry]{return poetry->systemContext();};
  batInputs.getSystemPoetryAsync=[poetry=poetry,loop=&owner](const gadgets::Gadgets& gadgets,
      const Json& modelName,const Json&,const Json& clients){
    if(!modelName.is_string())throw std::invalid_argument("Agent model ID required");
    Json names=Json::array();for(const auto* gadget:gadgets)if(gadget)
      names.push_back(Json{{"name",gadget->name}});
    event_loop::Deferred<Json> output(*loop);
    auto pending=poetry->systemPoetry(names,modelName.get<std::string>(),
      clients.is_array()?clients:Json::array());
    pending.then([output](const std::vector<std::string>* value,
        const std::exception_ptr& error){
      if(error)output.reject(error);
      else if(value)output.resolve(Json(*value));
      else output.reject(std::make_exception_ptr(
        std::logic_error("Agent prompt was not returned")));
    });return output;
  };
  batInputs.enhanceSystemPoetry=[poetry=poetry](const Json& value,
      const std::string& modelName,const Json& directories,const Json& gadgets){
    return poetry->enhanceBatPoetry(value,modelName,directories,gadgets);};
  batInputs.enqueueSdkEvent=[this](const Json& event){
    log("Agent progress: "+event.dump());};
  batInputs.enqueuePendingNotification=[this](const Json& value){
    if(runtime)runtime->queue().enqueuePendingNotification(value);};
  batInputs.killShellMissionsForBat=[missions=managedShell.missions](const std::string& id){
    missions->stopForBat(id);};
  batInputs.logError=[this](const std::exception& error){log(error.what());};
  batInputs.notifyCommandLifecycle=notifyLifecycle;
  bats=application_bats::IndigenousBatApplication::create(std::move(batInputs));
  preparedGadgets->factory.additionalBaseGadgets.push_back(bats->makeSendMessageGadget());
  missionBridge=application_mission_ui::BatMissionBridge::create(owner,store,
    managedShell.missions,[bats=bats](const std::string& id){
      bats->stopMission(id);},[this]{if(shell)shell->requestRedraw();});
  *generationBats=bats;
  skills->bindBatRunner([weak=std::weak_ptr(bats)](const bats::RunBatParams& params,
      const query::Yield& yield){
    const auto bat=weak.lock();
    if(!bat)throw std::runtime_error("Agent runtime has retired");
    bat->runForSkill(params,yield);
  });
  replaceJsonState(store,[definitions=bats->batDefinitions()](const Json& current){
    Json next=current;next["agentDefinitions"]=definitions;return next;});
  roster->publish(gadgets::live_roster::makeSnapshot(
    std::move(preparedGadgets->factory)));
  preparedGadgets.reset();
}
void Host::Impl::bindMcpRuntime(){
  if(!mcpClientRows)mcpClientRows=std::make_shared<Json>(Json::array());
  mcpAuthUi=std::make_shared<application_commands::McpAuthUi>();
  mcp::manager::IndigenousInputs inputs(owner);
  inputs.configHome=configurationDirectory(config);
  inputs.projectRoot=bindingProjectRoot();
  inputs.configurations=config.mcpServers;
  inputs.explicitConfigurations=config.mcpServersConfigured;
  inputs.gadgetServices=mcpServices;
  inputs.elicitationRequest=[controller=mcpElicitation,hooks=hooks](
      const std::string& server,const Json& request,std::stop_token signal){
    application_mcp_elicitation::Hooks lifecycle;
    lifecycle.before=[hooks](const std::string& server,const Json& params,
        std::stop_token signal){return hooks->elicitationBefore(server,params,signal);};
    lifecycle.after=[hooks](const std::string& server,const Json& params,
        const Json& raw,std::stop_token signal){
      return hooks->elicitationAfter(server,params,raw,signal);};
    return controller->request(server,request,signal,std::move(lifecycle));
  };
  const std::weak_ptr<gadgets::live_roster::Roster> weakGeneration=roster;
  const auto clientRows=mcpClientRows;
  const auto lastPoetryEntries=std::make_shared<Json>(Json::array());
  auto ownerRef=std::make_shared<std::weak_ptr<mcp::manager::IndigenousRuntime>>();
  inputs.changed=[this,weakGeneration,clientRows,lastPoetryEntries,ownerRef](
      const std::vector<mcp::manager::ServerResult>& servers){
    const auto generation=weakGeneration.lock();
    if(!generation)return;
    std::vector<gadgets::Gadget> gadgets;
    std::vector<std::shared_ptr<mcp::Connection>> connections;
    Json mcpCommands=Json::array(),gadgetRows=Json::array();
    for(const auto& server:servers){
      if(server.type!="connected")continue;
      if(server.connection)connections.push_back(server.connection);
      for(const auto& gadget:server.gadgets){
        gadgetRows.push_back(Json{{"name",gadget.name},{"server",server.name}});
        gadgets.push_back(gadget);
      }
      if(server.commands.is_array())for(const auto& command:server.commands)
        mcpCommands.push_back(command);
    }
    generation->replaceMcp(std::move(gadgets),std::move(connections));
    auto active=ownerRef->lock();
    if(!active)return;
    const auto clients=active->manager()->clients();
    *clientRows=clients;
    if(generation!=roster)return;
    const auto resources=active->manager()->resources();
    replaceJsonState(store,[clients,resources,mcpCommands,gadgetRows](
        const Json& current){
      Json next=current;
      Json mcp=value::property(next,"mcp");
      if(!mcp.is_object())mcp=Json::object();
      mcp["clients"]=clients;mcp["tools"]=gadgetRows;
      mcp["commands"]=mcpCommands;mcp["resources"]=resources;
      next["mcp"]=std::move(mcp);
      return next;
    });
    if(frontend&&commands){
      if(runtime&&*lastPoetryEntries!=mcpCommands){
        *lastPoetryEntries=mcpCommands;
        refreshCommandCatalog(generation);
      }else frontend->rebindEnvironment(config,poetry,commands->entries(),
        clients,fileHistory);
    }
    if(shell)shell->requestRedraw();
  };
  inputs.diagnostic=[this](const std::string& server,
      const std::string& message){log("MCP "+server+": "+message);};
  inputs.notification=[this,controller=mcpElicitation,hooks=hooks](const std::string& server,const Json& value){
    const auto& method=value::property(value,"method");
    if(method=="notifications/elicitation/complete"){
      const auto& id=value::property(value::property(value,"params"),"elicitationId");
      if(id.is_string()){
        controller->complete(server,id.get<std::string>());
        hooks->elicitationComplete(server,id.get<std::string>());
      }
    }
    log("MCP notification "+server+": "+
      (method.is_string()?method.get<std::string>():"event"));};
  inputs.authorizationUrl=[this,auth=mcpAuthUi](const std::string& server,
      const std::string& url){
    if(auth->server!=server)return;
    auth->url=url;
    if(auth->changed)auth->changed();
    else {statusText="MCP authorization URL: "+url;
      if(shell)shell->requestRedraw();}
  };
  inputs.waitingForCallback=[auth=mcpAuthUi](const std::string& server,
      std::function<void(const std::string&)> submit){
    if(auth->server!=server)return;
    auth->submitCallback=std::move(submit);
    if(auth->changed)auth->changed();
  };
  mcpRuntime=mcp::manager::IndigenousRuntime::create(std::move(inputs));
  *ownerRef=mcpRuntime;
  *mcpRuntimeSlot=mcpRuntime;
  mcp::StorageConfig skillStorage;
  skillStorage.getGadgetResultsDir=[storage=storage]{
    return gadget_results::getGadgetResultsDir(storage);};
  skillStorage.ensureGadgetResultsDir=[storage=storage]{
    std::filesystem::create_directories(
      gadget_results::getGadgetResultsDir(storage));};
  skillStorage.nowMs=platform::nowMillis;
  skillStorage.randomSuffix=messages->randomUUID;
  mcp::ResultServices resultServices;
  mcp::bindIndigenousResultServices(resultServices,skillStorage);
  skills->bindMcpPoetryRunner([manager=mcpRuntime,resultServices,
      aggregate=attachments,messages=messages,invoked=invokedSkills,
      dispatcher=owner.dispatcher(),loop=&owner](const Json& command,
      const std::string& args,const bats::BatContext& context){
    const auto result=application::awaitOnOwner<user_input::InputResult>(
      dispatcher,[manager,resultServices,aggregate,messages,invoked,
        loop,command,args,context]{
        auto getAttachments=[aggregate,context](const std::string& text){
          return aggregate->collectPoetry(text,Json(),Json::array(),
            value::property(context.query.data,"messages"),
            std::optional<std::string>("repl_main_thread"));};
        auto entry=application_commands::makeMcpPoetryCommand(manager,
          resultServices,getAttachments,messages,invoked,
          [context]{return std::make_shared<bats::BatContext>(context);},
          *loop,command);
        return entry.expandPoetry(args,Json::array(),Json::array(),{});
      });
    Json processed={{"messages",result.messages},
      {"shouldQuery",result.shouldQuery},{"command",command}};
    if(result.allowedGadgets)processed["allowedTools"]=*result.allowedGadgets;
    if(result.model)processed["model"]=*result.model;
    if(result.effort)processed["effort"]=*result.effort;
    return processed;
  });
}
void Host::Impl::refreshCommandCatalog(
    const std::shared_ptr<gadgets::live_roster::Roster>& generation){
  if(generation!=roster||!frontend||!commands||!runtime||closing)return;
  auto nextCommands=makeCommands();
  Json allRows=Json::array();
  for(const auto& entry:nextCommands->entries())
    allRows.push_back(entry.descriptor);
  const auto rows=commandRows;
  runtime->replaceCommands(nextCommands->entries(),
    [this,weak=std::weak_ptr(generation),nextCommands,rows,allRows]{
      if(weak.lock()!=roster)return;
      commands=nextCommands;
      *rows=allRows;
      if(poetry)poetry->clearPoetrySections();
      frontend->rebindEnvironment(config,poetry,nextCommands->entries(),
        mcpClientRows?*mcpClientRows:Json::array(),fileHistory);
      if(shell){shell->replaceCommandUi(nextCommands->router()->ui());
        shell->requestRedraw();}
    });
}
}
