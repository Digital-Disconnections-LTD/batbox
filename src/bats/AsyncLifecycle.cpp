#include "bats/AsyncLifecycle.hpp"
#include "bats/ResumeFilters.hpp"
#include <exception>
#include <stdexcept>

namespace batbox::bats {
namespace {
const Json& field(const Json& object,const char* key){return value::property(object,key);}
template<class T>void need(const T& callback,const char* key){
  if(!callback)throw std::invalid_argument(std::string("Unbound async agent import: ")+key);
}
Json spread(Json target,const Json& additions){
  if(additions.is_object())for(auto it=additions.begin();it!=additions.end();++it)
    target[it.key()]=it.value();
  return target;
}
}
void runAsyncBatLifecycle(const AsyncLifecycleParams& params,
  AsyncLifecycleImports& imports){
  std::function<void()> stopSummarization;
  Json batMessages=Json::array();
  BatMessageRefs messageRefs;
  const auto syncMessages=[&]{
    for(std::size_t i=0;i<messageRefs.size()&&i<batMessages.size();++i)
      batMessages[i]=*messageRefs[i];
  };
  std::exception_ptr escaping;
  try{
    try{
      need(imports.createProgressTracker,"createProgressTracker");
      Json tracker=imports.createProgressTracker();
      need(imports.createActivityDescriptionResolver,"createActivityDescriptionResolver");
      const auto resolveActivity=imports.createActivityDescriptionResolver(
        params.gadgetUseContext.query.gadgets);
      std::function<void(const std::shared_ptr<StashSafeParams>&)> onStashSafeParams;
      if(params.enableSummarization){
        onStashSafeParams=[&](const std::shared_ptr<StashSafeParams>& snapshot){
          need(imports.startBatSummarization,"startAgentSummarization");
          stopSummarization=imports.startBatSummarization(params.missionId,snapshot,
            params.rootSetAppState);
        };
      }
      need(params.makeStream,"makeStream");
      params.makeStream(onStashSafeParams,query::Yield::withReferences(
        [&](const query::MessageRef& messageRef){
        const Json& message=*messageRef;
        messageRefs.push_back(messageRef);
        batMessages.push_back(message);
        need(params.rootSetAppState,"rootSetAppState");
        params.rootSetAppState([&](const Json& previous){
          const Json& mission=field(field(previous,"tasks"),params.missionId.c_str());
          need(imports.isLocalBatMission,"isLocalAgentTask");
          if(!imports.isLocalBatMission(mission)||!value::truthy(field(mission,"retain")))
            return previous;
          Json next=previous;
          Json nextMission=mission;
          Json messages=field(mission,"messages");
          if(!messages.is_array())messages=Json::array();
          nextMission["messages"]=mergeBatTranscriptMessages(messages,Json::array({message}));
          next["tasks"][params.missionId]=std::move(nextMission);
          return next;
        });
        need(imports.updateProgressFromMessage,"updateProgressFromMessage");
        imports.updateProgressFromMessage(tracker,message,resolveActivity,
          params.gadgetUseContext.query.gadgets);
        need(imports.getProgressUpdate,"getProgressUpdate");
        Json progress=imports.getProgressUpdate(tracker);
        need(imports.updateAsyncBatProgress,"updateAsyncAgentProgress");
        imports.updateAsyncBatProgress(params.missionId,progress,params.rootSetAppState);
        const auto lastGadgetName=getLastGadgetUseName(message);
        if(lastGadgetName&&!lastGadgetName->empty()){
          Json emitProgress=imports.getProgressUpdate(tracker);
          const Json& activity=field(field(emitProgress,"lastActivity"),"activityDescription");
          Json event={{"taskId",params.missionId},
            {"description",activity.is_null()?Json(params.description):activity},
            {"startTime",params.metadata.startTime},
            {"totalTokens",field(emitProgress,"tokenCount")},
            {"toolUses",field(emitProgress,"toolUseCount")},
            {"lastToolName",*lastGadgetName}};
          const Json& gadgetUseId=field(params.gadgetUseContext.query.data,"toolUseId");
          if(!gadgetUseId.is_null())event["toolUseId"]=gadgetUseId;
          need(imports.emitMissionProgressEvent,"emitTaskProgressEvent");
          imports.emitMissionProgressEvent(event);
        }
      }));
      if(stopSummarization)stopSummarization();
      syncMessages();
      Json batResult=finalizeBatGadget(batMessages,params.missionId,
        params.metadata,imports.result);
      need(imports.completeAsyncBat,"completeAsyncAgent");
      imports.completeAsyncBat(batResult,messageRefs,params.rootSetAppState);
      need(imports.result.extractTextContent,"extractTextContent");
      std::string finalMessage=imports.result.extractTextContent(
        field(batResult,"content"),"\n");
      need(imports.feature,"feature");
      if(imports.feature("TRANSCRIPT_CLASSIFIER")){
        need(imports.classifyHandoffIfNeeded,"classifyHandoffIfNeeded");
        need(params.gadgetUseContext.getAppState,"toolUseContext.getAppState");
        const auto warning=imports.classifyHandoffIfNeeded(batMessages,
          params.gadgetUseContext.query.gadgets,
          field(effectiveBatGetAppState(params.gadgetUseContext)(),"toolPermissionContext"),
          params.abortOwner?params.abortOwner->get_token():std::stop_token{},
          params.metadata.batType,
          field(batResult,"totalToolUseCount").get<int>());
        if(warning&&!warning->empty())finalMessage=*warning+"\n\n"+finalMessage;
      }
      need(params.getWorktreeResult,"getWorktreeResult");
      Json worktree=params.getWorktreeResult();
      need(imports.getTokenCountFromTracker,"getTokenCountFromTracker");
      Json notice={{"taskId",params.missionId},{"description",params.description},
        {"status","completed"},{"finalMessage",finalMessage},
        {"usage",{{"totalTokens",imports.getTokenCountFromTracker(tracker)},
          {"toolUses",field(batResult,"totalToolUseCount")},
          {"durationMs",field(batResult,"totalDurationMs")}}}};
      const Json& gadgetUseId=field(params.gadgetUseContext.query.data,"toolUseId");
      if(!gadgetUseId.is_null())notice["toolUseId"]=gadgetUseId;
      notice=spread(std::move(notice),worktree);
      need(imports.enqueueBatNotification,"enqueueAgentNotification");
      imports.enqueueBatNotification(notice,params.rootSetAppState);
    }catch(...){
      const auto cause=std::current_exception();
      if(stopSummarization)stopSummarization();
      syncMessages();
      bool aborted=false;
      try{std::rethrow_exception(cause);}catch(const BatAbortError&){aborted=true;}
      catch(...){}
      if(aborted){
        need(imports.killAsyncBat,"killAsyncAgent");
        imports.killAsyncBat(params.missionId,messageRefs,params.rootSetAppState);
        need(params.getWorktreeResult,"getWorktreeResult");
        const Json worktree=params.getWorktreeResult();
        const auto partial=extractPartialResult(batMessages,imports.result);
        Json notice={{"taskId",params.missionId},{"description",params.description},
          {"status","killed"}};
        const Json& gadgetUseId=field(params.gadgetUseContext.query.data,"toolUseId");
        if(!gadgetUseId.is_null())notice["toolUseId"]=gadgetUseId;
        if(partial)notice["finalMessage"]=*partial;
        notice=spread(std::move(notice),worktree);
        need(imports.enqueueBatNotification,"enqueueAgentNotification");
        imports.enqueueBatNotification(notice,params.rootSetAppState);
      }else{
        need(imports.errorMessage,"errorMessage");
        const std::string message=imports.errorMessage(cause);
        need(imports.failAsyncBat,"failAsyncAgent");
        imports.failAsyncBat(params.missionId,message,messageRefs,params.rootSetAppState);
        need(params.getWorktreeResult,"getWorktreeResult");
        const Json worktree=params.getWorktreeResult();
        Json notice={{"taskId",params.missionId},{"description",params.description},
          {"status","failed"},{"error",message}};
        const Json& gadgetUseId=field(params.gadgetUseContext.query.data,"toolUseId");
        if(!gadgetUseId.is_null())notice["toolUseId"]=gadgetUseId;
        notice=spread(std::move(notice),worktree);
        need(imports.enqueueBatNotification,"enqueueAgentNotification");
        imports.enqueueBatNotification(notice,params.rootSetAppState);
      }
    }
  }catch(...){escaping=std::current_exception();}
  need(imports.clearInvokedSkillsForBat,"clearInvokedSkillsForAgent");
  imports.clearInvokedSkillsForBat(params.batIdForCleanup);
  if(escaping)std::rethrow_exception(escaping);
}
} // namespace batbox::bats
