#include "application_bats/IndigenousBatApplication.hpp"
#include "mcp/manager/IndigenousRuntime.hpp"
#include "application/Async.hpp"
#include "bats/ResumeFilters.hpp"
#include "normalization/Normalization.hpp"
#include "cwd/Override.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/Platform.hpp"
#include "gadget_results/Budget.hpp"
#include <stdexcept>

namespace batbox::application_bats {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string string(const Json& value){return value.is_string()?value.get<std::string>():"";}
}
event_loop::Deferred<event_loop::Unit> IndigenousBatApplication::submitToMission(
    const std::string& id,const std::string& text){
  return submitToMissionWithContext(id,text,{}, {},true);
}
event_loop::Deferred<event_loop::Unit> IndigenousBatApplication::submitToMissionWithContext(
    const std::string& value,const std::string& text,
    std::optional<bats::BatContext> context,IndigenousQueryBridge::CanUseGadget canUse,bool echo,
    std::vector<Json> queuedInputs){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Agent submission requires owner");
  event_loop::Deferred<event_loop::Unit> output(inputs_.owner);
  try{
    if(closing_)throw std::runtime_error("Agent application is shutting down");
    const auto resolved=resolveMissionId(value);
    if(!resolved)throw std::runtime_error("No agent found: "+value);
    const auto id=*resolved;
    const auto state=appState();const auto& mission=value::property(field(state,"tasks"),id);
    if(!mission.is_null()&&!bats::isPanelBatMission(mission))
      throw std::runtime_error("Messages require an ordinary child agent: "+id);
    if(field(mission,"status")=="running"){
      queueDirectInput(id,text,echo);
      bats::queuePendingMessage(id,text,missionState_);
      output.resolve({});return output;
    }
    // Reserve the identity before the asynchronous read. A stopped worker must
    // finish its finally/notification before another run can reuse this ID.
    {
      std::lock_guard lock(conversationMutex_);
      if(pendingResumes_.contains(id))throw std::runtime_error("Agent resume already in progress: "+id);
      if(!context){
        if(const auto at=conversations_.find(id);at!=conversations_.end()){
          context=at->second.context;if(!canUse)canUse=at->second.canUse;
        }
      }
    }
    if(!context&&inputs_.currentBatContext)context=inputs_.currentBatContext();
    if(!context)throw std::runtime_error("Parent agent context is unavailable for resume: "+id);
    auto self=shared_from_this();
    if(!canUse)canUse=[weak=weak_from_this()](const Json& input){
      const auto self=weak.lock();
      if(!self){
        std::promise<Json> failed;auto future=failed.get_future();
        failed.set_exception(std::make_exception_ptr(std::runtime_error("Native Agent owner is unavailable")));
        return future;
      }
      return self->inputs_.permissions->check("Agent",input,
        field(self->appState(),"toolPermissionContext"));
    };
    // Context callbacks are live root imports, never a stopped child's abort
    // token or a no-op child setter. History comes from the sidechain below.
    context->getAppState=[weak=weak_from_this()]{
      auto self=weak.lock();if(!self)throw std::runtime_error("Agent owner closed");return self->appState();
    };
    context->setAppState=missionState_.setAppState;
    context->setAppStateForMissions=missionState_.setAppState;
    const auto owner=inputs_.owner.dispatcher();
    context->getTypedAppState=[owner,store=inputs_.store]{
      return application::onOwnerAndWait<app_state::AppStateRef>(owner,[store]{return store->getState();});
    };
    context->setTypedAppState=[owner,store=inputs_.store](const app_state::AppStateUpdater& update){
      application::onOwnerAndWait<void>(owner,[store,update]{store->setState(update);});
    };
    context->setTypedAppStateForMissions=context->setTypedAppState;
    context->abortOwner=std::make_shared<std::stop_source>();
    context->query.stop=context->abortOwner->get_token();
    context->query.abortReason.clear();
    context->currentAbortReason=[]{return std::string{};};
    context->query.gadgets=inputs_.currentGadgets();
    if(inputs_.currentGadgetOwner)context->query.gadgetSnapshotOwner=inputs_.currentGadgetOwner();
    context->query.data["options"]["agentDefinitions"]=batDefinitions();
    context->query.data["options"]["mainLoopModel"]=inputs_.config.model;
    const auto mcp=inputs_.batMcpRuntime?inputs_.batMcpRuntime():nullptr;
    context->query.data["options"]["mcpClients"]=mcp?mcp->manager()->clients():
      field(field(state,"mcp"),"clients");
    context->query.data.erase("agentId");
    context->query.data["messages"]=Json::array();
    bats::materializeAppStateModifier(*context);
    {std::lock_guard lock(conversationMutex_);pendingResumes_.insert(id);}
    if(queuedInputs.empty())queueDirectInput(id,text,echo&&bats::isPanelBatMission(mission));
    if(const auto at=pendingDirectInputs_.find(id);at!=pendingDirectInputs_.end()){
      queuedInputs=std::move(at->second);pendingDirectInputs_.erase(at);
    }
    (void)bats::drainPendingMessages(id,missionState_);
    ++conversationJobs_;
    application::runInWorker<event_loop::Unit>(inputs_.owner,
      [self,id,text,context=*context,canUse=std::move(canUse),queuedInputs]()mutable{
        {
          std::unique_lock lock(self->conversationMutex_);
          self->conversationChanged_.wait(lock,[&]{
            return self->closing_||!self->batJobs_.contains(id);
          });
          if(self->closing_)throw std::runtime_error("Agent application is shutting down");
        }
        (void)self->resumeMission(id,text,std::move(context),std::move(canUse),queuedInputs);
        return event_loop::Unit{};
      }).then([self,id,output,queuedInputs](const event_loop::Unit*,const std::exception_ptr& error){
        {std::lock_guard lock(self->conversationMutex_);self->pendingResumes_.erase(id);}
        if(error){
          auto waiting=queuedInputs;
          if(const auto at=self->pendingDirectInputs_.find(id);at!=self->pendingDirectInputs_.end())
            waiting.insert(waiting.end(),at->second.begin(),at->second.end());
          self->pendingDirectInputs_[id]=std::move(waiting);
        }
        --self->conversationJobs_;self->checkShutdown();
        if(error)output.reject(error);else output.resolve({});
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
bats::ResumeResult IndigenousBatApplication::resumeMission(const std::string& id,
    const std::string& text,bats::BatContext context,IndigenousQueryBridge::CanUseGadget canUse,
    const std::vector<Json>& queuedInputs){
  if(inputs_.owner.isOwnerThread())throw std::logic_error("Agent history resume requires worker");
  requireCapacity(context.query.stop);
  bool launched=false,registered=false;
  try{
    auto self=shared_from_this();
    const auto indigenous=makeServices();
    bats::ResumeServices services;
    services.nowMillis=platform::nowMillis;
    const Json canonical=queuedInputs.empty()?Json():queuedInputs.back();
    services.getBatTranscript=[self,canonical](const std::string& id){
      return runtime::contextAsync(std::launch::async,[self,id,canonical]{
        auto transcript=self->readTranscript(id);
        if(transcript&&!canonical.is_null()){
          Json prefix=Json::array();
          for(const auto& message:field(*transcript,"messages"))
            if(field(message,"uuid")!=field(canonical,"uuid"))prefix.push_back(message);
          (*transcript)["messages"]=std::move(prefix);
        }
        return transcript;
      });
    };
    services.readBatMetadata=[self](const std::string& id){
      return runtime::contextAsync(std::launch::async,[self,id]{
        auto paths=self->pathsForBat(id);std::lock_guard lock(self->pathsMutex_);
        return paths->readBatMetadata(id);
      });
    };
    services.filterUnresolvedGadgetUses=bats::filterUnresolvedGadgetUses;
    services.filterOrphanedThinkingOnlyMessages=normalization::filterOrphanedThinkingOnlyMessages;
    services.filterWhitespaceOnlyAssistantMessages=[messages=inputs_.messages](const Json& values){
      normalization::Dependencies deps;deps.messageServices=messages.get();
      return normalization::filterWhitespaceOnlyAssistantMessages(values,deps);
    };
    services.reconstructForSubagentResumeIndigenous=gadget_results::reconstructForSubagentResume;
    services.isDirectory=[](const std::string& path){return std::filesystem::is_directory(runtime::pathFromUtf8(path));};
    services.touchWorktree=[](const std::string& path){
      std::filesystem::last_write_time(runtime::pathFromUtf8(path),std::filesystem::file_time_type::clock::now());
    };
    services.additionalWorkingDirectoryKeys=indigenous.launch.poetry.additionalWorkingDirectoryKeys;
    services.generalPurposeBatDefinition=batDefinitions()["activeAgents"][0];
    services.forkBatDefinition=Json{{"agentType","fork"}};
    services.createUserMessage=canonical.is_null()?indigenous.launch.createUserMessage:
      std::function<Json(const Json&)>([canonical](const Json&){return canonical;});
    services.getSystemPoetry=indigenous.launch.poetry.getSystemPoetry;
    services.buildEffectiveSystemPoetry=indigenous.launch.poetry.buildEffectiveSystemPoetry;
    services.getBatModel=indigenous.launch.getBatModel;
    services.assembleGadgetPool=indigenous.launch.assembleGadgetPool;
    services.isBuiltInBat=indigenous.launch.isBuiltInBat;
    services.getQuerySourceForBat=indigenous.launch.getQuerySourceForBat;
    services.registerAsyncBat=[self,&registered](const Json& value,const bats::RootSetAppState& set){
      bats::BatRegistrationParams params;
      params.batId=string(field(value,"agentId"));params.description=string(field(value,"description"));
      params.poetry=string(field(value,"prompt"));params.selectedBat=field(value,"selectedAgent");
      params.model=string(field(value,"model"));
      if(field(value,"toolUseId").is_string())params.gadgetUseId=field(value,"toolUseId").get<std::string>();
      auto state=self->missionState_;state.setAppState=set;
      auto registration=self->registration_;registration.state=&state;
      // Cleanup captures the permanent owner state rather than the temporary
      // adapter above. This preserves cancellation after resumeMission returns.
      registration.registerCleanup=[self,id=params.batId](const std::function<void()>&){
        return self->registration_.registerCleanup([self,id]{bats::killAsyncBat(id,self->missionState_);});
      };
      auto mission=bats::registerAsyncBat(params,registration);
      registered=true;
      return bats::RegisteredAsyncBat{params.batId,mission.abortOwner};
    };
    services.getParentFlightId=[self,id]{return self->parentFlightId(id);};
    services.runWithBatContext=indigenous.async.runWithBatContext;
    services.runWithCwdOverride=indigenous.async.runWithCwdOverride;
    services.isCoordinatorMode=[]{return false;};
    services.isForkSubagentEnabled=[]{return false;};
    services.getSdkBatProgressSummariesEnabled=[]{return false;};
    services.getMissionOutputPath=registration_.getMissionOutputPath;
    services.runAsyncBatLifecycle=[self,run=indigenous.foreground.runBat,&launched](const bats::ResumeLaunch& launch){
      bats::AsyncLifecycleParams lifecycle;
      lifecycle.missionId=launch.mission.batId;lifecycle.abortOwner=launch.mission.abortOwner;
      lifecycle.description=launch.description;lifecycle.batIdForCleanup=launch.batIdForCleanup;
      lifecycle.gadgetUseContext=launch.parentContext;lifecycle.rootSetAppState=launch.rootSetAppState;
      lifecycle.enableSummarization=launch.enableSummarization;
      const auto& metadata=launch.metadata;
      lifecycle.metadata.poetry=string(field(metadata,"prompt"));
      lifecycle.metadata.resolvedBatModel=string(field(metadata,"resolvedAgentModel"));
      lifecycle.metadata.batType=string(field(metadata,"agentType"));
      lifecycle.metadata.isBuiltInBat=field(metadata,"isBuiltInAgent")==true;
      lifecycle.metadata.isAsync=true;lifecycle.metadata.startTime=field(metadata,"startTime").get<std::int64_t>();
      lifecycle.makeStream=[launch,run](const auto& onStash,const query::Yield& yield){
        run(launch.makeRunBatParams(onStash),yield);
      };
      lifecycle.getWorktreeResult=[path=launch.worktreePath]{
        return path?Json{{"worktreePath",*path}}:Json::object();
      };
      self->launchLifecycle(std::move(lifecycle));launched=true;
    };
    return bats::resumeBatBackground(bats::ResumeParams{id,text,std::move(context),std::move(canUse),{}},services);
  }catch(...){
    const auto error=std::current_exception();
    if(registered&&!launched){
      std::string message="Agent resume failed";
      try{std::rethrow_exception(error);}catch(const std::exception& failure){message=failure.what();}catch(...){}
      try{bats::failBatMission(id,message,missionState_);}catch(...){}
    }
    if(!launched)rollbackCapacity();
    std::rethrow_exception(error);
  }
}
} // namespace batbox::application_bats
