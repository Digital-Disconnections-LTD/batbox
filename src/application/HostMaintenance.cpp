#include "application/HostInternal.hpp"
#include "profiler/Indigenous.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::application {
namespace {
bool shellBusy(const application_shell::Host& shell){
  return shell.shell&&!shell.shell->activeJobIds().empty();
}
event_loop::Deferred<event_loop::Unit> ready(event_loop::Loop& owner){
  event_loop::Deferred<event_loop::Unit> result(owner);result.resolve({});return result;
}
event_loop::Deferred<event_loop::Unit> rejected(event_loop::Loop& owner,
    std::exception_ptr error){
  event_loop::Deferred<event_loop::Unit> result(owner);result.reject(error);return result;
}
void restoreProjection(const std::shared_ptr<app_state::AppStateStore>& store,
    const Json& definitions,const Json& mcp,const Json& settings){
  store->setState([&](const app_state::AppStateRef& previous){
    const auto before=previous->snapshot();
    if(value::property(before,"agentDefinitions")==definitions&&
        value::property(before,"mcp")==mcp&&
        value::property(before,"settings")==settings)return previous;
    auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
    next->fields->set("agentDefinitions",definitions);
    next->fields->set("mcp",mcp);
    next->fields->set("settings",settings);
    return next;
  });
}
}

std::filesystem::path Host::Impl::bindingProjectRoot()const{
  return bindingProjectRootOverride.value_or(
    roost?roost->projectRoot():config.cwd);
}
Host::Impl::RetiredRoost Host::Impl::captureRoostGeneration()const{
  RetiredRoost result;
  result.config=config;result.projectRoot=bindingProjectRoot();
  if(store){const auto state=appState();
    result.settings=value::property(state,"settings");
    result.mcpSnapshot=value::property(state,"mcp");}
  result.poetry=poetry;result.skills=skills;result.fileHistory=fileHistory;
  result.missionBridge=missionBridge;result.managedShell=managedShell;
  result.storage=storage;result.hooks=hooks;result.compactor=compactor;
  result.engine=engine;result.bats=bats;result.commands=commands;
  result.attachments=attachments;result.mcpRuntime=mcpRuntime;
  result.roster=roster;result.gadgets=roster?roster->current():nullptr;
  result.memory=memory;result.lsp=lsp;
  result.generationEngineSlot=generationEngineSlot;
  result.generationBatSlot=generationBatSlot;
  result.mcpRuntimeSlot=mcpRuntimeSlot;result.mcpClientRows=mcpClientRows;
  result.commandRows=commandRows;result.mcpAuthUi=mcpAuthUi;
  result.mcpServices=mcpServices;
  if(bats)result.transcriptOrigins=bats->transcriptOrigins();
  return result;
}
void Host::Impl::installRoostGeneration(const RetiredRoost& generation){
  config=generation.config;
  poetry=generation.poetry;skills=generation.skills;
  fileHistory=generation.fileHistory;missionBridge=generation.missionBridge;
  managedShell=generation.managedShell;storage=generation.storage;
  hooks=generation.hooks;compactor=generation.compactor;
  engine=generation.engine;bats=generation.bats;
  commands=generation.commands;attachments=generation.attachments;
  mcpRuntime=generation.mcpRuntime;roster=generation.roster;
  memory=generation.memory;lsp=generation.lsp;
  generationEngineSlot=generation.generationEngineSlot;
  generationBatSlot=generation.generationBatSlot;
  mcpRuntimeSlot=generation.mcpRuntimeSlot;
  mcpClientRows=generation.mcpClientRows;
  commandRows=generation.commandRows;mcpAuthUi=generation.mcpAuthUi;
  mcpServices=generation.mcpServices;
}

Json Host::Impl::runtimeMemorySummary()const{
  if(!owner.isOwnerThread())throw std::logic_error("Runtime summary requires owner");
  std::size_t live=1,dormant=0,contexts=0,streamBytes=0;
  const auto accumulate=[&](const auto& generation){
    if(!generation)return;
    const auto state=generation->retainedStateSummary();
    const Json& context=value::property(state,"retainedChildContexts");
    const Json& bytes=value::property(state,"retainedStreamBytes");
    if(context.is_number())contexts+=static_cast<std::size_t>(context.get<double>());
    if(bytes.is_number())streamBytes+=static_cast<std::size_t>(bytes.get<double>());
  };
  accumulate(bats);
  for(const auto& generation:retiredRoosts){
    if(generation.state==RetiredRoost::State::Dormant)++dormant;
    else ++live;
    accumulate(generation.bats);
  }
  const auto memory=profiler::indigenousMemoryUsage();
  return Json{{"liveWorkspaceGenerations",live},
    {"dormantWorkspaceRecords",dormant},
    {"retainedChildContexts",contexts},
    {"retainedStreamBytes",streamBytes},
    {"residentBytes",memory.rss},
    {"heapUsedBytes",memory.heapUsed}};
}

void Host::Impl::beginRetiredShutdown(std::size_t index){
  if(index>=retiredRoosts.size())return;
  auto& generation=retiredRoosts[index];
  if(generation.state!=RetiredRoost::State::Live)return;
  generation.state=RetiredRoost::State::Closing;
  generation.shutdownReady.emplace(owner);
  if(generation.bats)generation.transcriptOrigins=
    generation.bats->transcriptOrigins();
  // Registration sentinel prevents a synchronously failed shutdown call from
  // completing the record while later shutdown owners are still being added.
  auto remaining=std::make_shared<int>(1);
  auto problem=std::make_shared<bool>(false);
  const auto settle=[this,index,remaining,problem](const char* label,
      std::exception_ptr error){
    if(error){*problem=true;log(std::string("Retired ")+label+" shutdown failed");}
    if(--*remaining!=0)return;
    if(index>=retiredRoosts.size())return;
    auto& retired=retiredRoosts[index];
    const auto finished=retired.shutdownReady;
    retired.poetry.reset();retired.skills.reset();retired.fileHistory.reset();
    retired.missionBridge.reset();retired.managedShell={};retired.storage={};
    retired.hooks.reset();retired.compactor.reset();retired.engine.reset();
    retired.bats.reset();retired.commands.reset();retired.attachments.reset();
    retired.mcpRuntime.reset();retired.roster.reset();retired.gadgets.reset();
    retired.memory.reset();retired.lsp.reset();
    retired.generationEngineSlot.reset();retired.generationBatSlot.reset();
    retired.mcpRuntimeSlot.reset();retired.mcpClientRows.reset();
    retired.commandRows.reset();retired.mcpAuthUi.reset();
    retired.mcpServices={};retired.mcpReady.reset();
    retired.shutdownReady.reset();
    retired.state=RetiredRoost::State::Dormant;
    if(*problem)log("Retired workspace kept as dormant identity after shutdown errors");
    if(finished)finished->resolve({});
  };
  const auto started=[remaining]{++*remaining;};
  if(generation.memory){
    started();try{generation.memory->shutdown().then([settle](const event_loop::Unit*,
        const std::exception_ptr& error){settle("memory",error);});}
    catch(...){settle("memory",std::current_exception());}
  }
  if(generation.bats){
    started();try{generation.bats->shutdown().then([settle](const event_loop::Unit*,
        const std::exception_ptr& error){settle("agent",error);});}
    catch(...){settle("agent",std::current_exception());}
  }
  if(generation.hooks){
    started();try{generation.hooks->finalizeAsyncHooks().then([settle](
        const event_loop::Unit*,const std::exception_ptr& error){settle("hook",error);});}
    catch(...){settle("hook",std::current_exception());}
  }
  if(generation.managedShell.shell){
    started();try{application_shell::stopAndWait(owner,generation.managedShell).then(
      [settle](const event_loop::Unit*,const std::exception_ptr& error){
        settle("shell",error);});}
    catch(...){settle("shell",std::current_exception());}
  }
  if(generation.mcpRuntime){
    started();try{generation.mcpRuntime->shutdown().then([settle](
        const event_loop::Unit*,const std::exception_ptr& error){settle("MCP",error);});}
    catch(...){settle("MCP",std::current_exception());}
  }
  if(generation.lsp){
    started();try{
      auto future=generation.lsp->shutdownAsync();
      owner.queueWork([future]{future.get();},[settle](std::exception_ptr error){
        settle("LSP",error);});
    }catch(...){settle("LSP",std::current_exception());}
  }
  settle("registered",{});
}

void Host::Impl::releaseRetiredProviders(){
  if(!owner.isOwnerThread()||closing||bindingRetired)return;
  const bool currentIdle=(!runtime||runtime->canReplaceConversation())&&
    (!bats||!bats->hasActiveJobs())&&(!memory||!memory->hasActiveJobs())&&
    (!hooks||!hooks->hasActiveJobs())&&!shellBusy(managedShell);
  if(currentIdle&&roster)roster->releaseRetiredAtQuiescence();
  bool pending=false;
  const auto observed=shell?shell->viewState().batId:std::optional<std::string>{};
  for(std::size_t index=0;index<retiredRoosts.size();++index){
    auto& generation=retiredRoosts[index];
    if(generation.state!=RetiredRoost::State::Live)continue;
    if(generation.pendingOperations){pending=true;continue;}
    if(generation.bats){
      generation.transcriptOrigins=generation.bats->transcriptOrigins();
      const auto state=appState();const auto& missions=value::property(state,"tasks");
      for(const auto& [id,_]:generation.transcriptOrigins){
        const auto& mission=value::property(missions,id);
        if(value::property(mission,"retain")==true&&
            (!observed||*observed!=id)&&
            value::property(mission,"status")!="running")
          generation.bats->releaseMission(id);
      }
      generation.bats->releaseInactiveState();
    }
    if((generation.bats&&generation.bats->hasActiveJobs())||
        (generation.memory&&generation.memory->hasActiveJobs())||
        (generation.hooks&&generation.hooks->hasActiveJobs())||
        shellBusy(generation.managedShell)){
      pending=true;continue;
    }
    if(generation.roster)generation.roster->releaseRetiredAtQuiescence();
    beginRetiredShutdown(index);
  }
  if(pending&&!retirementPoll){
    retirementPoll=owner.setTimeout(2000,[this]{
      retirementPoll.reset();releaseRetiredProviders();},false);
  }
}

std::shared_ptr<application_bats::IndigenousBatApplication>
Host::Impl::reviveRetired(std::size_t index){
  if(!owner.isOwnerThread())throw std::logic_error("Agent revival requires owner");
  if(index>=retiredRoosts.size())throw std::out_of_range("Retired workspace index");
  auto& dormant=retiredRoosts[index];
  if(dormant.state==RetiredRoost::State::Live)return dormant.bats;
  if(dormant.state!=RetiredRoost::State::Dormant)
    throw std::runtime_error("Old workspace is still shutting down");
  const auto identity=dormant;
  auto active=captureRoostGeneration();
  const auto state=appState();
  const Json definitions=value::property(state,"agentDefinitions");
  const Json mcp=value::property(state,"mcp");
  const Json settings=value::property(state,"settings");
  bindingRetired=true;bindingProjectRootOverride=identity.projectRoot;
  dormant.state=RetiredRoost::State::Reviving;
  try{
    RetiredRoost blank;blank.config=identity.config;
    installRoostGeneration(blank);
    restoreProjection(store,definitions,identity.mcpSnapshot,identity.settings);
    roster=std::make_shared<gadgets::live_roster::Roster>();
    bindStorage();bindFileHistory();bindRoostProviders();
    bindBackend();bindMcpRuntime();
    commands=makeCommands();
    auto revived=captureRoostGeneration();
    revived.transcriptOrigins=identity.transcriptOrigins;
    revived.pendingOperations=identity.pendingOperations;
    revived.bats->restoreTranscriptOrigins(identity.transcriptOrigins);
    revived.state=RetiredRoost::State::Live;
    revived.mcpReady=mcpRuntime->start();
    installRoostGeneration(active);
    bindingProjectRootOverride.reset();bindingRetired=false;
    restoreProjection(store,definitions,mcp,settings);
    dormant=std::move(revived);
    return dormant.bats;
  }catch(...){
    auto partial=captureRoostGeneration();
    installRoostGeneration(active);
    bindingProjectRootOverride.reset();bindingRetired=false;
    restoreProjection(store,definitions,mcp,settings);
    dormant.state=RetiredRoost::State::Dormant;
    if(partial.bats||partial.memory||partial.hooks||partial.managedShell.shell||
        partial.mcpRuntime||partial.lsp){
      retiredRoosts.push_back(std::move(partial));
      beginRetiredShutdown(retiredRoosts.size()-1);
    }
    throw;
  }
}

std::shared_ptr<application_bats::IndigenousBatApplication>
Host::Impl::batOwner(const std::string& value){
  if(!owner.isOwnerThread())throw std::logic_error("Agent lookup requires owner");
  const auto id=bats?bats->resolveMissionId(value):std::optional<std::string>{};
  const auto& target=id?*id:value;
  if(bats&&bats->ownsMission(target))return bats;
  for(std::size_t i=0;i<retiredRoosts.size();++i){
    const auto& generation=retiredRoosts[i];
    if((generation.bats&&generation.bats->ownsMission(target))||
        generation.transcriptOrigins.contains(target)){
      if(generation.state==RetiredRoost::State::Closing)
        return generation.bats;
      return reviveRetired(i);
    }
  }
  return bats;
}
std::optional<std::size_t> Host::Impl::retiredIndexForBat(
    const std::string& value)const{
  const auto id=bats?bats->resolveMissionId(value):std::optional<std::string>{};
  const auto& target=id?*id:value;
  if(bats&&bats->ownsMission(target))return {};
  for(std::size_t i=0;i<retiredRoosts.size();++i){
    const auto& generation=retiredRoosts[i];
    if((generation.bats&&generation.bats->ownsMission(target))||
        generation.transcriptOrigins.contains(target))return i;
  }
  return {};
}
void Host::Impl::finishRetiredBatOperation(std::optional<std::size_t> index){
  if(!index||*index>=retiredRoosts.size())return;
  auto& generation=retiredRoosts[*index];
  if(!generation.pendingOperations)throw std::logic_error(
    "Retired agent operation count underflow");
  --generation.pendingOperations;
  owner.queueNextTurn([this]{releaseRetiredProviders();});
}
event_loop::Deferred<event_loop::Unit> Host::Impl::batOwnerReady(
    const std::string& id){
  try{
    const auto resolved=bats?bats->resolveMissionId(id):std::optional<std::string>{};
    const auto& target=resolved?*resolved:id;
    for(const auto& generation:retiredRoosts){
      if(!generation.transcriptOrigins.contains(target)||
          generation.state!=RetiredRoost::State::Closing||
          !generation.shutdownReady)continue;
      event_loop::Deferred<event_loop::Unit> result(owner);
      generation.shutdownReady->then([this,id,result](const event_loop::Unit*,
          const std::exception_ptr& error){
        if(error){result.reject(error);return;}
        batOwnerReady(id).then([result](const event_loop::Unit*,
            const std::exception_ptr& nextError){
          if(nextError)result.reject(nextError);else result.resolve({});
        });
      });
      return result;
    }
    auto ownerBat=batOwner(id);
    if(!ownerBat)throw std::runtime_error("Agent owner is unavailable");
    for(const auto& generation:retiredRoosts)
      if(generation.bats==ownerBat&&generation.mcpReady)
        return *generation.mcpReady;
    return ready(owner);
  }catch(...){return rejected(owner,std::current_exception());}
}
event_loop::Deferred<Json> Host::Impl::observeBatMission(const std::string& id){
  event_loop::Deferred<Json> result(owner);
  const auto index=retiredIndexForBat(id);
  if(index)++retiredRoosts[*index].pendingOperations;
  batOwnerReady(id).then([this,id,index,result](const event_loop::Unit*,
      const std::exception_ptr& error){
    if(error){finishRetiredBatOperation(index);result.reject(error);return;}
    try{
      batOwner(id)->observeMission(id).then([this,index,result](const Json* value,
          const std::exception_ptr& failure){
        finishRetiredBatOperation(index);
        if(failure)result.reject(failure);
        else if(value)result.resolve(*value);
        else result.reject(std::make_exception_ptr(std::runtime_error(
          "Agent observation returned no task")));
      });
    }catch(...){finishRetiredBatOperation(index);result.reject(std::current_exception());}
  });
  return result;
}
event_loop::Deferred<event_loop::Unit> Host::Impl::submitBatMessage(
    const std::string& id,const std::string& text){
  event_loop::Deferred<event_loop::Unit> result(owner);
  const auto index=retiredIndexForBat(id);
  if(index)++retiredRoosts[*index].pendingOperations;
  batOwnerReady(id).then([this,id,text,index,result](const event_loop::Unit*,
      const std::exception_ptr& error){
    if(error){finishRetiredBatOperation(index);result.reject(error);return;}
    try{
      batOwner(id)->submitToMission(id,text).then([this,index,result](const event_loop::Unit*,
          const std::exception_ptr& failure){
        finishRetiredBatOperation(index);
        if(failure)result.reject(failure);else result.resolve({});
      });
    }catch(...){finishRetiredBatOperation(index);result.reject(std::current_exception());}
  });
  return result;
}
} // namespace batbox::application
