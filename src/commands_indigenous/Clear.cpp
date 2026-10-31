#include "commands_indigenous/Clear.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::commands_indigenous {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
bool shouldKill(const Json& mission){
  return mission.is_object()&&mission.contains("isBackgrounded")&&
    field(mission,"isBackgrounded")==false;
}
void required(bool value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Clear command import required: ")+name);
}
struct PreservedBat {std::string missionId,batId;bool running=false;};
struct Run:std::enable_shared_from_this<Run> {
  ClearServices services;
  event_loop::Deferred<event_loop::Unit> output;
  std::vector<std::string> preservedIds;
  std::vector<PreservedBat> localBats;
  explicit Run(ClearServices imports):services(std::move(imports)),
    output(services.loop){}
  void fail(std::exception_ptr error){output.reject(error);}
  void preserve(const std::string& id){
    if(std::find(preservedIds.begin(),preservedIds.end(),id)==preservedIds.end())
      preservedIds.push_back(id);
  }
  void start(){
    try{
      required(bool(services.flightEndHookTimeoutMs),"sessionEndHookTimeoutMs");
      required(bool(services.flightEndHooks),"sessionEndHooks");
      services.flightEndHooks(services.flightEndHookTimeoutMs()).then(
        [self=shared_from_this()](const event_loop::Unit*,
            const std::exception_ptr& error){
          if(error){self->fail(error);return;}
          try{self->afterEndHooks();}
          catch(...){self->fail(std::current_exception());}
        });
    }catch(...){fail(std::current_exception());}
  }
  void scanPreserved(){
    if(!services.getAppState)return;
    const auto state=services.getAppState();
    if(!state||!state->missions)return;
    for(const auto& [missionId,ref]:state->missions->entries()){
      if(!ref||shouldKill(*ref))continue;
      required(bool(services.isLocalBatMission),"isLocalAgentTask");
      required(bool(services.isInProcessPartnerMission),"isInProcessTeammateTask");
      if(services.isLocalBatMission(*ref)){
        const auto& bat=field(*ref,"agentId");
        if(!bat.is_string())continue;
        const auto id=bat.get<std::string>();
        preserve(id);
        const auto& missionOwnId=field(*ref,"id");
        localBats.push_back({missionOwnId.is_string()?missionOwnId.get<std::string>():
          missionId,id,field(*ref,"status")=="running"});
      }else if(services.isInProcessPartnerMission(*ref)){
        const auto& bat=field(field(*ref,"identity"),"agentId");
        if(bat.is_string())preserve(bat.get<std::string>());
      }
    }
  }
  void updateAppState(){
    if(!services.setAppState)return;
    required(bool(services.createEmptyAttributionState),
      "createEmptyAttributionState");
    services.setAppState([self=shared_from_this()](
        const app_state::AppStateRef& prior){
      if(!prior)throw std::invalid_argument("Clear command AppState missing");
      auto next=std::make_shared<app_state::AppState>(prior->shallowCopy());
      next->missions=std::make_shared<app_state::OrderedStringMap<value::ValueRef>>();
      Json missionSnapshot=Json::object();
      if(prior->missions)for(const auto& [missionId,ref]:prior->missions->entries()){
        if(!ref)continue;
        if(!shouldKill(*ref)){
          next->missions->set(missionId,ref);
          missionSnapshot[missionId]=*ref;
          continue;
        }
        try{
          if(field(*ref,"status")=="running"){
            required(bool(self->services.killForegroundMission),"killForegroundTask");
            self->services.killForegroundMission(missionId,*ref);
          }
        }catch(...){
          if(self->services.logError)
            self->services.logError(std::current_exception());
        }
        required(bool(self->services.evictMissionOutput),"evictTaskOutput");
        self->services.evictMissionOutput(missionId);
      }
      next->fields->set("tasks",std::move(missionSnapshot));
      next->fields->set("attribution",
        self->services.createEmptyAttributionState());
      next->fields->setRef("standaloneAgentContext",{});
      next->trackedFiles=std::make_shared<runtime::OrderedStringSet>();
      next->fields->set("fileHistory",Json{{"snapshots",Json::array()},
        {"trackedFiles",Json::array()},{"snapshotSequence",0}});
      const auto priorSnapshot=prior->snapshot();
      const auto& priorMcp=field(priorSnapshot,"mcp");
      Json mcp={{"clients",Json::array()},{"tools",Json::array()},
        {"commands",Json::array()},{"resources",Json::object()}};
      if(priorMcp.is_object()&&priorMcp.contains("pluginReconnectKey"))
        mcp["pluginReconnectKey"]=priorMcp.at("pluginReconnectKey");
      next->fields->set("mcp",std::move(mcp));
      return next;
    });
  }
  void afterEndHooks(){
    scanPreserved();
    required(bool(services.setMessages),"setMessages");
    services.setMessages(Json::array());
    required(bool(services.feature),"feature");
    if(services.feature("PROACTIVE")||services.feature("KAIROS")){
      required(bool(services.setContextBlocked),"setContextBlocked");
      services.setContextBlocked(false);
    }
    if(services.setConversationId){
      required(bool(services.randomUUID),"randomUUID");
      services.setConversationId(services.randomUUID());
    }
    required(bool(services.clearFlightStashes),"clearSessionCaches");
    services.clearFlightStashes(preservedIds);
    required(bool(services.setCwd)&&bool(services.getOriginalCwd),"setCwd/getOriginalCwd");
    services.setCwd(services.getOriginalCwd());
    required(bool(services.readFileState),"readFileState");
    services.readFileState->clear();
    if(services.discoveredSkillNames)services.discoveredSkillNames->clear();
    if(services.loadedNestedMemoryPaths)services.loadedNestedMemoryPaths->clear();
    updateAppState();
    required(bool(services.clearAllPlanSlugs),"clearAllPlanSlugs");
    services.clearAllPlanSlugs();
    required(bool(services.clearFlightMetadata),"clearSessionMetadata");
    services.clearFlightMetadata();
    required(bool(services.regenerateFlightId),"regenerateSessionId");
    services.regenerateFlightId(true);
    if(services.internalFlightEnvPresent&&services.internalFlightEnvPresent()){
      required(bool(services.setEnvironmentFlightId)&&bool(services.getFlightId),
        "setEnvironmentSessionId/getSessionId");
      services.setEnvironmentFlightId(services.getFlightId());
    }
    required(bool(services.resetFlightFilePointer),"resetSessionFilePointer");
    services.resetFlightFilePointer().then(
      [self=shared_from_this()](const event_loop::Unit*,
          const std::exception_ptr& error){
        if(error){self->fail(error);return;}
        try{self->afterPointerReset();}
        catch(...){self->fail(std::current_exception());}
      });
  }
  void afterPointerReset(){
    for(const auto& mission:localBats){
      if(!mission.running)continue;
      required(bool(services.initMissionOutputAsSymlink)&&
        bool(services.getBatTranscriptPath),"Mission output symlink");
      services.initMissionOutputAsSymlink(mission.missionId,
        services.getBatTranscriptPath(mission.batId));
    }
    if(services.feature("COORDINATOR_MODE")){
      required(bool(services.isCoordinatorMode)&&bool(services.saveMode),
        "coordinator mode persistence");
      services.saveMode(services.isCoordinatorMode()?"coordinator":"normal");
    }
    required(bool(services.getCurrentWorktreeFlight),"getCurrentWorktreeSession");
    if(auto worktree=services.getCurrentWorktreeFlight()){
      required(bool(services.saveWorktreeState),"saveWorktreeState");
      services.saveWorktreeState(*worktree);
    }
    required(bool(services.flightStartHooks),"sessionStartHooks");
    services.flightStartHooks().then(
      [self=shared_from_this()](const Json* messages,
          const std::exception_ptr& error){
        if(error){self->fail(error);return;}
        try{
          if(messages&&messages->is_array()&&!messages->empty())
            self->services.setMessages(*messages);
          self->output.resolve(event_loop::Unit{});
        }catch(...){self->fail(std::current_exception());}
      });
  }
};
}
event_loop::Deferred<event_loop::Unit> clearConversation(ClearServices services){
  auto run=std::make_shared<Run>(std::move(services));
  run->start();return run->output;
}
user_input::CommandEntry makeClearCommand(std::shared_ptr<ClearServices> services){
  if(!services)throw std::invalid_argument("Clear command services required");
  user_input::CommandEntry entry;
  entry.descriptor={{"type","local"},{"name","clear"},
    {"description","Clear conversation history and free up context"},
    {"aliases",Json::array({"reset","new"})},
    {"supportsNonInteractive",false}};
  entry.loadLocal=[services]{
    event_loop::Deferred<user_input::LocalCall> loaded(services->loop);
    loaded.resolve([services](const std::string&){
      event_loop::Deferred<Json> output(services->loop);
      try{clearConversation(*services).then(
        [output](const event_loop::Unit*,const std::exception_ptr& error){
          if(error){output.reject(error);return;}
          output.resolve(Json{{"type","text"},{"value",""}});
        });}
      catch(...){output.reject(std::current_exception());}
      return output;
    });
    return loaded;
  };
  return entry;
}
} // namespace batbox::commands_indigenous
