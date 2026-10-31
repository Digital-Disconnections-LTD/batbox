#include "bats/BatGadgetBackgroundContinuation.hpp"
#include "runtime/MessageSnapshot.hpp"
#include <stdexcept>

namespace batbox::bats {
namespace {
const Json& field(const Json& object,const char* key){return value::property(object,key);}
std::string text(const Json& value){return value.is_string()?value.get<std::string>():"";}
template<class F>void need(const F& fn,const char* name){
  if(!fn)throw std::invalid_argument(std::string("BatGadget continuation requires ")+name);
}
Json materialize(const BatMessageRefs& refs){
  Json result=Json::array();
  for(const auto& ref:refs)result.push_back(runtime::snapshotMessage(ref));
  return result;
}
Json spread(Json base,const Json& extra){
  if(extra.is_object())for(auto it=extra.begin();it!=extra.end();++it)
    base[it.key()]=it.value();
  return base;
}
} // namespace
void continueBatGadgetInBackground(BatGadgetBackgroundContinuation state,
    BatGadgetBackgroundServices& services){
  need(services.runBat,"runAgent");
  need(services.getSdkBatProgressSummariesEnabled,
    "getSdkAgentProgressSummariesEnabled");
  need(services.updateAsyncBatProgress,"updateAsyncAgentProgress");
  need(services.completeAsyncBat,"completeAsyncAgent");
  need(services.killAsyncBat,"killAsyncAgent");
  need(services.failAsyncBat,"failAsyncAgent");
  need(services.enqueueBatNotification,"enqueueAgentNotification");
  need(services.clearInvokedSkillsForBat,"clearInvokedSkillsForAgent");
  need(services.result.extractTextContent,"extractTextContent");
  need(services.errorMessage,"errorMessage");
  std::function<void()> stopBackgroundedSummarization;
  std::exception_ptr deferred;
  try{
  try{
    if(state.returnForegroundIterator)state.returnForegroundIterator();
    else if(state.foregroundStream)
      state.foregroundStream->returnWithin(std::chrono::milliseconds(1000));
    Json tracker=createProgressTracker();
    const auto resolveActivity=createActivityDescriptionResolver(
      state.context.query.gadgets);
    for(const auto& ref:state.existingMessages)
      updateProgressFromMessage(tracker,runtime::snapshotMessage(ref),
        resolveActivity,state.context.query.gadgets,services.progress);
    RunBatParams run=state.runBatParams;
    run.isAsync=true;
    run.override["agentId"]=state.missionId;
    run.overrideAbortOwner=state.missionAbortOwner;
    if(services.getSdkBatProgressSummariesEnabled()){
      need(services.startBatSummarization,"startAgentSummarization");
      run.onStashSafeParams=[&](const std::shared_ptr<StashSafeParams>& params){
        stopBackgroundedSummarization=services.startBatSummarization(
          state.missionId,state.missionId,params,state.rootSetAppState);
      };
    }
    services.runBat(run,query::Yield::withReferences(
      [&](const query::MessageRef& ref){
        const Json message=runtime::snapshotMessage(ref);
        state.existingMessages.push_back(ref);
        updateProgressFromMessage(tracker,message,resolveActivity,
          state.context.query.gadgets,services.progress);
        services.updateAsyncBatProgress(state.missionId,getProgressUpdate(tracker),
          state.rootSetAppState);
        const auto lastGadgetName=getLastGadgetUseName(message);
        if(lastGadgetName){
          need(services.emitMissionProgress,"emitTaskProgress");
          services.emitMissionProgress(tracker,state.missionId,state.gadgetUseId,
            state.description,state.metadata.startTime,*lastGadgetName);
        }
      }));
    const Json messages=materialize(state.existingMessages);
    Json batResult=finalizeBatGadget(messages,state.missionId,state.metadata,
      services.result);
    // The mission status transition precedes classification and worktree cleanup.
    services.completeAsyncBat(batResult,state.existingMessages,
      state.rootSetAppState);
    std::string finalMessage=services.result.extractTextContent(
      field(batResult,"content"),"\n");
    need(services.feature,"feature");
    if(services.feature("TRANSCRIPT_CLASSIFIER")){
      need(services.classifyHandoffIfNeeded,"classifyHandoffIfNeeded");
      need(state.context.getAppState,"toolUseContext.getAppState");
      const Json current=effectiveBatGetAppState(state.context)();
      const auto warning=services.classifyHandoffIfNeeded(messages,
        state.context.query.gadgets,
        field(current,"toolPermissionContext"),
        state.missionAbortOwner?state.missionAbortOwner->get_token():std::stop_token{},
        text(field(state.selectedBat,"agentType")),
        field(batResult,"totalToolUseCount").get<int>());
      if(warning)finalMessage=*warning+"\n\n"+finalMessage;
    }
    Json worktree=Json::object();
    if(state.cleanupWorktree)worktree=state.cleanupWorktree();
    else if(state.worktree)worktree=cleanupBatWorktreeIfNeeded(*state.worktree,
      services.worktree);
    Json notification={{"taskId",state.missionId},{"description",state.description},
      {"status","completed"},{"finalMessage",finalMessage},
      {"usage",{{"totalTokens",getTokenCountFromTracker(tracker)},
        {"toolUses",field(batResult,"totalToolUseCount")},
        {"durationMs",field(batResult,"totalDurationMs")}}}};
    if(!state.gadgetUseId.is_null())notification["toolUseId"]=state.gadgetUseId;
    services.enqueueBatNotification(spread(std::move(notification),worktree),
      state.rootSetAppState);
  }catch(const BatAbortError&){
    services.killAsyncBat(state.missionId,state.existingMessages,
      state.rootSetAppState);
    Json worktree=Json::object();
    if(state.cleanupWorktree)worktree=state.cleanupWorktree();
    else if(state.worktree)worktree=cleanupBatWorktreeIfNeeded(*state.worktree,
      services.worktree);
    const auto partial=extractPartialResult(materialize(state.existingMessages),
      services.result);
    Json notification={{"taskId",state.missionId},{"description",state.description},
      {"status","killed"}};
    if(!state.gadgetUseId.is_null())notification["toolUseId"]=state.gadgetUseId;
    if(partial)notification["finalMessage"]=*partial;
    services.enqueueBatNotification(spread(std::move(notification),worktree),
      state.rootSetAppState);
  }catch(...){
    const std::string error=services.errorMessage(std::current_exception());
    services.failAsyncBat(state.missionId,error,state.existingMessages,
      state.rootSetAppState);
    Json worktree=Json::object();
    if(state.cleanupWorktree)worktree=state.cleanupWorktree();
    else if(state.worktree)worktree=cleanupBatWorktreeIfNeeded(*state.worktree,
      services.worktree);
    Json notification={{"taskId",state.missionId},{"description",state.description},
      {"status","failed"},{"error",error}};
    if(!state.gadgetUseId.is_null())notification["toolUseId"]=state.gadgetUseId;
    services.enqueueBatNotification(spread(std::move(notification),worktree),
      state.rootSetAppState);
  }
  }catch(...){deferred=std::current_exception();}
  try{
    if(stopBackgroundedSummarization)stopBackgroundedSummarization();
    services.clearInvokedSkillsForBat(state.syncBatId);
  }catch(...){deferred=std::current_exception();}

  if(deferred)std::rethrow_exception(deferred);
}
} // namespace batbox::bats
