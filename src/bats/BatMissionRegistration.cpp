#include "bats/BatMissionRegistration.hpp"
#include <stdexcept>

namespace batbox::bats {
namespace {
const Json& field(const Json& object,const char* key){return value::property(object,key);}
template<class T>void need(const T& callback,const char* key){
  if(!callback)throw std::invalid_argument(std::string("Unbound agent task import: ")+key);
}
void copyIfPresent(Json& target,const Json& source,const char* key){
  if(source.is_object()&&source.contains(key))target[key]=source[key];
  else target.erase(key);
}
}
Json createMissionStateBase(const std::string& id,const std::string& type,
  const std::string& description,const std::optional<std::string>& gadgetUseId,
  BatRegistrationServices& services){
  if(!services.state)throw std::invalid_argument("Unbound agent task state");
  need(services.state->nowMillis,"nowMillis");
  need(services.getMissionOutputPath,"getTaskOutputPath");
  Json base={{"id",id},{"type",type},{"status","pending"},
    {"description",description},{"startTime",services.state->nowMillis()},
    {"outputFile",services.getMissionOutputPath(id)},{"outputOffset",0},
    {"notified",false}};
  if(gadgetUseId)base["toolUseId"]=*gadgetUseId;
  return base;
}
void registerMission(const Json& mission,BatRegistrationServices& services){
  if(!services.state)throw std::invalid_argument("Unbound agent task state");
  need(services.state->setAppState,"setAppState");
  const Json& idValue=field(mission,"id");
  if(!idValue.is_string())throw std::invalid_argument("Missing task id");
  const std::string id=idValue.get<std::string>();
  bool replacement=false;
  services.state->setAppState([&](const Json& previous){
    const Json& existing=value::property(field(previous,"tasks"),id);
    replacement=!existing.is_null();
    Json merged=mission;
    if(existing.is_object()&&existing.contains("retain")){
      for(const char* key:{"retain","startTime","messages",
                            "diskLoaded","pendingMessages"})
        copyIfPresent(merged,existing,key);
    }
    Json next=previous;
    next["tasks"][id]=std::move(merged);
    return next;
  });
  if(replacement)return;
  need(services.enqueueSdkEvent,"enqueueSdkEvent");
  Json event={{"type","system"},{"subtype","task_started"},
    {"task_id",id},{"description",field(mission,"description")},
    {"task_type",field(mission,"type")}};
  const Json& gadgetUseId=field(mission,"toolUseId");
  if(!gadgetUseId.is_null())event["tool_use_id"]=gadgetUseId;
  if(mission.is_object()&&mission.contains("workflowName"))
    event["workflow_name"]=mission["workflowName"];
  if(mission.is_object()&&mission.contains("prompt"))event["prompt"]=mission["prompt"];
  services.enqueueSdkEvent(event);
}
RegisteredLocalBat registerAsyncBat(const BatRegistrationParams& params,
  BatRegistrationServices& services){
  if(!services.state)throw std::invalid_argument("Unbound agent task state");
  need(services.getBatTranscriptPath,"getAgentTranscriptPath");
  need(services.initMissionOutputAsSymlink,"initTaskOutputAsSymlink");
  services.initMissionOutputAsSymlink(params.batId,
    services.getBatTranscriptPath(params.batId));
  RegisteredLocalBat result;
  if(services.onCreateAbortOwner)
    services.onCreateAbortOwner(static_cast<bool>(params.parentAbortOwner));
  result.abortOwner=std::make_shared<std::stop_source>();
  if(params.parentAbortOwner){
    result.parentAbortLink=std::make_shared<std::stop_callback<std::function<void()>>>(
      params.parentAbortOwner->get_token(),
      [owner=result.abortOwner]{owner->request_stop();});
  }
  Json mission=createMissionStateBase(params.batId,"local_agent",params.description,
    params.gadgetUseId,services);
  const Json& batType=field(params.selectedBat,"agentType");
  mission["type"]="local_agent";
  mission["status"]="running";
  mission["agentId"]=params.batId;
  mission["prompt"]=params.poetry;
  mission["selectedAgent"]=params.selectedBat;
  mission["agentType"]=batType.is_string()?batType:Json("general-purpose");
  mission["abortController"]=Json::object();
  mission["retrieved"]=false;
  mission["lastReportedToolCount"]=0;
  mission["lastReportedTokenCount"]=0;
  mission["isBackgrounded"]=true;
  mission["pendingMessages"]=Json::array();
  mission["retain"]=false;
  mission["diskLoaded"]=false;
  need(services.registerCleanup,"registerCleanup");
  result.unregisterCleanup=services.registerCleanup(
    [id=params.batId,state=services.state]{killAsyncBat(id,*state);});
  result.missionState=mission;
  registerMission(mission,services);
  if(services.onRegistered)services.onRegistered(params.batId,result);
  return result;
}
} // namespace batbox::bats
