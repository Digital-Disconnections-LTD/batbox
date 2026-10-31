#include "application_bats/IndigenousBatApplication.hpp"
#include "application_bats/Worktree.hpp"
#include "application/Async.hpp"
#include "bats/BatResultBindings.hpp"
#include "bats/BatGadgetBackgroundContinuation.hpp"
#include "cwd/Override.hpp"
#include "runtime/AsyncLaunch.hpp"
#include "runtime/Platform.hpp"
#include <stdexcept>

namespace batbox::application_bats {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string errorText(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::exception& value){return value.what();}
  catch(...){return "Subagent failed";}
  return "";
}
double assistantTextLength(const Json& message){
  const auto& blocks=field(field(message,"message"),"content");
  double length=0;
  if(blocks.is_array())for(const auto& block:blocks)
    if(field(block,"type")=="text"&&field(block,"text").is_string())
      length+=value::stringLength(field(block,"text").get<std::string>());
  return length;
}
}
bats::BatGadgetCallServices IndigenousBatApplication::makeServices(){
  bats::BatGadgetCallServices gadget;
  bindPreflight(gadget);bindLaunch(gadget);bindForeground(gadget);bindAsync(gadget);
  WorktreeInputs worktree(inputs_.owner);
  worktree.config=inputs_.config;
  worktree.hooks=inputs_.hooks;
  const auto weak=weak_from_this();
  worktree.getSettings=[weak]{
    if(const auto self=weak.lock()){
      const Json state=self->appState();
      return Json(field(state,"settings"));
    }
    return Json::object();
  };
  worktree.enqueueBatMetadata=[weak](const std::string& id,const Json& data){
    if(const auto self=weak.lock()){
      auto paths=self->pathsForBat(id);
      std::lock_guard lock(self->pathsMutex_);
      paths->writeBatMetadata(id,data);
    }
  };
  worktree.diagnostic=[log=inputs_.logError](const std::string& message){
    if(log)log(std::runtime_error(message));
  };
  bindBatWorktree(gadget,std::move(worktree));
  BatServicesInputs inputs;
  inputs.gadget=std::move(gadget);
  inputs.preflight=makeRunPreflight();
  inputs.setup=makeRunSetup();
  inputs.loop=makeRunLoop();
  inputs.query=query_;
  auto services=makeBatGadgetServices(std::move(inputs));
  const auto run=services.foreground.runBat;
  services.foreground.runBat=[self=shared_from_this(),run](
      const bats::RunBatParams& original,const query::Yield& yield){
    auto params=original;
    std::shared_ptr<std::stop_callback<std::function<void()>>> parentLink;
    const auto& id=field(params.override,"agentId");
    if(!params.overrideAbortOwner&&id.is_string()){
      if(auto abort=self->missionRuntime_.abortOwner(id.get<std::string>())){
        // Foreground registration has a separate indigenous stop sidecar. Link it
        // to this query so stopping the selected child does not cancel leader.
        params.overrideAbortOwner=abort;
        parentLink=std::make_shared<std::stop_callback<std::function<void()>>>(
          original.gadgetUseContext.query.stop,[abort]{abort->request_stop();});
      }
    }
    const auto missionId=id.is_string()?id.get<std::string>():std::string{};
    if(!missionId.empty())self->startBatJob(missionId);
    try{run(params,yield);}
    catch(...){if(!missionId.empty())self->finishBatJob(missionId);throw;}
    if(!missionId.empty())self->finishBatJob(missionId);
  };
  return services;
}
void IndigenousBatApplication::bindForeground(bats::BatGadgetCallServices& gadget){
  auto& foreground=gadget.foreground;
  foreground.foreground=&foreground_;
  foreground.nowMillis=platform::nowMillis;
  foreground.isBackgroundMissionsDisabled=[]{return false;};
  foreground.getSdkBatProgressSummariesEnabled=[]{return false;};
  foreground.getAutoBackgroundMs=[]{return 0.0;};
  foreground.getParentFlightId=[get=inputs_.currentFlightId]{
    return std::optional<std::string>(get());
  };
  foreground.isBuiltInBat=[](const Json& bat){
    return field(bat,"source")=="built-in";
  };
  foreground.runWithBatContext=[](const Json&,const std::function<void()>& run){
    run();
  };
  foreground.runWithCwdOverride=[](const std::string& path,
      const std::function<void()>& run){cwd::runWithCwdOverride(path,run);};
  foreground.renderBackgroundHint=[](){return Json("Agent is still running");};
  foreground.getMissionOutputPath=registration_.getMissionOutputPath;
  foreground.reportForegroundError=[log=inputs_.logError](std::exception_ptr error){
    if(!log||!error)return;
    try{std::rethrow_exception(error);}
    catch(const std::exception& value){log(value);}
    catch(...){log(std::runtime_error("Unknown foreground agent failure"));}
  };
  auto weak=weak_from_this();
  foreground.launchBackgroundContinuation=[weak](
      bats::BatGadgetBackgroundContinuation state){
    if(auto self=weak.lock())self->launchContinuation(std::move(state));
  };
  auto& progress=foreground.progress;
  progress.normalizeMessages=[messages=inputs_.messages](const Json& value){
    return messages::normalizeMessages(value,*messages);
  };
  progress.updateProgressFromMessage=[self=shared_from_this()](Json& tracker,
      const Json& message,const bats::ActivityResolver& resolver,
      const gadgets::Gadgets& gadgets){
    bats::updateProgressFromMessage(tracker,message,resolver,gadgets,self->progress_);
  };
  progress.emitMissionProgress=[emit=inputs_.enqueueSdkEvent](const Json& tracker,
      const std::string& missionId,const Json& gadgetUseId,const std::string& description,
      std::int64_t started,const std::string& lastGadget){
    Json event={{"taskId",missionId},{"description",description},
      {"startTime",started},{"lastToolName",lastGadget},
      {"totalTokens",bats::getTokenCountFromTracker(tracker)},
      {"toolUses",field(tracker,"toolUseCount")}};
    if(!gadgetUseId.is_null())event["toolUseId"]=gadgetUseId;
    emit(event);
  };
  progress.getSdkBatProgressSummariesEnabled=[]{return false;};
  progress.getProgressUpdate=bats::getProgressUpdate;
  progress.updateAsyncBatProgress=[self=shared_from_this()](
      const std::string& id,const Json& value,
      const bats::RootSetAppState& set){
    auto state=self->missionState_;state.setAppState=set;
    bats::updateBatProgress(id,value,state);
  };
  progress.getAssistantMessageContentLength=assistantTextLength;
  auto& final=foreground.final;
  final.result=bats::indigenousBatResultImports();
  final.getProgressUpdate=bats::getProgressUpdate;
  final.enqueueSdkEvent=inputs_.enqueueSdkEvent;
  final.clearInvokedSkillsForBat=[](const std::string&){};
  final.nowMillis=platform::nowMillis;
  final.isSyntheticMessage=[](const Json& message){
    return value::truthy(field(message,"isSynthetic"));
  };
  final.feature=[](const std::string&){return false;};
}
void IndigenousBatApplication::bindAsync(bats::BatGadgetCallServices& gadget){
  auto& asynchronous=gadget.async;
  asynchronous.registration=&registration_;
  asynchronous.getParentFlightId=[get=inputs_.currentFlightId]{
    return std::optional<std::string>(get());
  };
  asynchronous.isBuiltInBat=[](const Json& bat){
    return field(bat,"source")=="built-in";
  };
  asynchronous.isForkSubagentEnabled=[]{return false;};
  asynchronous.getSdkBatProgressSummariesEnabled=[]{return false;};
  asynchronous.runWithBatContext=[](const Json&,
      const std::function<void()>& run){run();};
  asynchronous.runWithCwdOverride=[](const std::string& path,
      const std::function<void()>& run){cwd::runWithCwdOverride(path,run);};
  asynchronous.launchLifecycle=[self=shared_from_this()](
      bats::AsyncLifecycleParams params){self->launchLifecycle(std::move(params));};
  asynchronous.getMissionOutputPath=registration_.getMissionOutputPath;
}
void IndigenousBatApplication::launchLifecycle(bats::AsyncLifecycleParams params){
  auto self=shared_from_this();
  auto keepAlive=beginBackgroundJob();
  const auto missionId=params.missionId;startBatJob(missionId);
  try{runtime::contextThread([self,keepAlive,missionId,params=std::move(params)]()mutable{
    try{
    bats::AsyncLifecycleImports imports;
    imports.result=bats::indigenousBatResultImports();
    bats::bindLocalBatMissionLifecycle(imports,self->missionState_,
      self->notification_,self->progress_);
    imports.feature=[](const std::string&){return false;};
    imports.errorMessage=errorText;
    imports.clearInvokedSkillsForBat=[](const std::string&){};
    try{bats::runAsyncBatLifecycle(params,imports);}
    catch(const std::exception& error){
      if(self->inputs_.logError)self->inputs_.logError(error);
    }
    }catch(const std::exception& error){
      if(self->inputs_.logError)try{self->inputs_.logError(error);}catch(...){}
    }catch(...){}
    self->releaseCapacity();
    self->finishBatJob(missionId);
    self->finishBackgroundJob(keepAlive);
  }).detach();}catch(...){finishBatJob(missionId);finishBackgroundJob(keepAlive);throw;}
}
void IndigenousBatApplication::launchContinuation(
    bats::BatGadgetBackgroundContinuation state){
  auto self=shared_from_this();
  auto keepAlive=beginBackgroundJob();
  const auto missionId=state.missionId;startBatJob(missionId);
  try{runtime::contextThread([self,keepAlive,missionId,state=std::move(state)]()mutable{
    try{
    bats::BatGadgetBackgroundServices services;
    const auto indigenous=self->makeServices();
    services.runBat=indigenous.foreground.runBat;
    services.worktree=indigenous.async.worktree;
    services.result=bats::indigenousBatResultImports();
    services.progress=self->progress_;
    services.emitMissionProgress=self->makeServices().foreground.progress.emitMissionProgress;
    services.updateAsyncBatProgress=[self](const std::string& id,
        const Json& progress,const bats::RootSetAppState& set){
      auto adapter=self->missionState_;adapter.setAppState=set;
      bats::updateBatProgress(id,progress,adapter);
    };
    services.getSdkBatProgressSummariesEnabled=[]{return false;};
    services.completeAsyncBat=[self](const Json& value,
        const bats::BatMessageRefs& refs,const bats::RootSetAppState& set){
      auto adapter=self->missionState_;adapter.setAppState=set;
      bats::completeBatMission(value,adapter,&refs);
    };
    services.killAsyncBat=[self](const std::string& id,
        const bats::BatMessageRefs& refs,const bats::RootSetAppState& set){
      auto adapter=self->missionState_;adapter.setAppState=set;
      bats::killAsyncBat(id,adapter,&refs);
    };
    services.failAsyncBat=[self](const std::string& id,const std::string& error,
        const bats::BatMessageRefs& refs,const bats::RootSetAppState& set){
      auto adapter=self->missionState_;adapter.setAppState=set;
      bats::failBatMission(id,error,adapter,&refs);
    };
    services.enqueueBatNotification=[self](const Json& value,
        const bats::RootSetAppState& set){
      auto adapter=self->notification_;adapter.setAppState=set;
      bats::enqueueBatNotification(value,adapter);
    };
    services.feature=[](const std::string&){return false;};
    services.errorMessage=errorText;
    services.clearInvokedSkillsForBat=[](const std::string&){};
    try{bats::continueBatGadgetInBackground(std::move(state),services);}
    catch(const std::exception& error){
      if(self->inputs_.logError)self->inputs_.logError(error);
    }
    }catch(const std::exception& error){
      if(self->inputs_.logError)try{self->inputs_.logError(error);}catch(...){}
    }catch(...){}
    self->releaseCapacity();
    self->finishBatJob(missionId);
    self->finishBackgroundJob(keepAlive);
  }).detach();}catch(...){finishBatJob(missionId);finishBackgroundJob(keepAlive);throw;}
}
} // namespace batbox::application_bats
