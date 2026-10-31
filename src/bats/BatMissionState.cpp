#include "bats/BatMissionState.hpp"
#include "bats/ResumeFilters.hpp"
#include <stdexcept>

namespace batbox::bats {
namespace {
const Json& field(const Json& object,const char* key){return value::property(object,key);}
const Json& missionIn(const Json& state,const std::string& id){
  return value::property(field(state,"tasks"),id);
}
template<class T>void need(const T& callback,const char* key){
  if(!callback)throw std::invalid_argument(std::string("Unbound LocalAgentTask import: ")+key);
}
using Edit=std::function<std::optional<Json>(const Json&)>;
void updateMission(const std::string& id,BatMissionStateServices& services,const Edit& edit){
  need(services.setAppState,"setAppState");
  services.setAppState([&](const Json& previous){
    const Json& current=missionIn(previous,id);
    if(current.is_null())return previous;
    auto updated=edit(current);
    if(!updated)return previous;
    Json next=previous;
    next["tasks"][id]=std::move(*updated);
    return next;
  });
}
void materialize(Json& mission,const BatMessageRefs* refs){
  if(!refs||!value::truthy(field(mission,"retain")))return;
  Json messages=Json::array();
  for(const auto& ref:*refs)messages.push_back(*ref);
  // Indigenous stream emissions carry shared references; materializing their final
  // values must preserve the disk prefix and locally echoed follow-up poetryEntries.
  mission["messages"]=mergeBatTranscriptMessages(field(mission,"messages"),messages);
}
void terminalFields(Json& mission,const std::string& status,
  BatMissionStateServices& services){
  need(services.nowMillis,"nowMillis");
  mission["status"]=status;
  mission["endTime"]=services.nowMillis();
  if(mission.contains("streamingText"))mission["streamingText"]=nullptr;
  if(mission.contains("streamingToolUses"))mission["streamingToolUses"]=Json::array();
  if(auto thinking=mission.find("streamingThinking");thinking!=mission.end()&&thinking->is_object()){
    (*thinking)["isStreaming"]=false;
    (*thinking)["streamingEndedAt"]=services.nowMillis();
  }
  if(value::truthy(field(mission,"retain")))mission.erase("evictAfter");
  else mission["evictAfter"]=services.nowMillis()+services.panelGraceMs;
  mission.erase("abortController");
  mission.erase("unregisterCleanup");
  mission.erase("selectedAgent");
}
}
bool isLocalBatMission(const Json& mission){
  return mission.is_object()&&field(mission,"type")=="local_agent";
}
bool isPanelBatMission(const Json& mission){
  return isLocalBatMission(mission)&&field(mission,"agentType")!="main-session";
}
void queuePendingMessage(const std::string& id,const std::string& message,
  BatMissionStateServices& services){
  updateMission(id,services,[&](const Json& mission)->std::optional<Json>{
    Json next=mission;
    Json pending=field(mission,"pendingMessages");
    if(!pending.is_array())pending=Json::array();
    pending.push_back(message);next["pendingMessages"]=std::move(pending);
    return next;
  });
}
void appendMessageToLocalBat(const std::string& id,const Json& message,
  BatMissionStateServices& services){
  updateMission(id,services,[&](const Json& mission)->std::optional<Json>{
    Json next=mission;
    Json messages=field(mission,"messages");
    if(!messages.is_array())messages=Json::array();
    messages.push_back(message);next["messages"]=std::move(messages);
    return next;
  });
}
Json drainPendingMessages(const std::string& id,BatMissionStateServices& services){
  need(services.getAppState,"getAppState");
  const Json mission=missionIn(services.getAppState(),id);
  const Json& pending=field(mission,"pendingMessages");
  if(!isLocalBatMission(mission)||!pending.is_array()||pending.empty())
    return Json::array();
  const Json drained=pending;
  updateMission(id,services,[](const Json& current)->std::optional<Json>{
    Json next=current;next["pendingMessages"]=Json::array();return next;
  });
  return drained;
}
void killAsyncBat(const std::string& id,BatMissionStateServices& services,
  const BatMessageRefs* refs){
  bool killed=false;
  updateMission(id,services,[&](const Json& mission)->std::optional<Json>{
    if(field(mission,"status")!="running")return std::nullopt;
    killed=true;
    need(services.abortMission,"abortTask");services.abortMission(id);
    need(services.unregisterCleanup,"unregisterCleanup");
    services.unregisterCleanup(id);
    Json next=mission;materialize(next,refs);
    terminalFields(next,"killed",services);
    return next;
  });
  if(killed){need(services.evictMissionOutput,"evictTaskOutput");
    services.evictMissionOutput(id);}
}
void killAllRunningBatMissions(const Json& missions,BatMissionStateServices& services){
  if(!missions.is_object())return;
  const Json snapshot=missions;
  for(auto it=snapshot.begin();it!=snapshot.end();++it)
    if(isLocalBatMission(it.value())&&field(it.value(),"status")=="running")
      killAsyncBat(it.key(),services);
}
void markBatsNotified(const std::string& id,BatMissionStateServices& services){
  updateMission(id,services,[](const Json& mission)->std::optional<Json>{
    if(value::truthy(field(mission,"notified")))return std::nullopt;
    Json next=mission;next["notified"]=true;return next;
  });
}
void updateBatProgress(const std::string& id,const Json& progress,
  BatMissionStateServices& services){
  updateMission(id,services,[&](const Json& mission)->std::optional<Json>{
    if(field(mission,"status")!="running")return std::nullopt;
    Json next=mission,updated=progress;
    const Json& summary=field(field(mission,"progress"),"summary");
    if(value::truthy(summary))updated["summary"]=summary;
    next["progress"]=std::move(updated);return next;
  });
}
void updateBatSummary(const std::string& id,const std::string& summary,
  BatMissionStateServices& services){
  std::optional<Json> captured;
  updateMission(id,services,[&](const Json& mission)->std::optional<Json>{
    if(field(mission,"status")!="running")return std::nullopt;
    const Json& progress=field(mission,"progress");
    captured=Json{{"tokenCount",field(progress,"tokenCount").is_null()?Json(0):field(progress,"tokenCount")},
      {"toolUseCount",field(progress,"toolUseCount").is_null()?Json(0):field(progress,"toolUseCount")},
      {"startTime",field(mission,"startTime")}};
    const Json& gadgetUseId=field(mission,"toolUseId");
    if(!gadgetUseId.is_null())(*captured)["toolUseId"]=gadgetUseId;
    Json next=mission;
    Json updated=progress.is_object()?progress:Json::object();
    updated["toolUseCount"]=(*captured)["toolUseCount"];
    updated["tokenCount"]=(*captured)["tokenCount"];
    updated["summary"]=summary;
    next["progress"]=std::move(updated);return next;
  });
  if(captured){
    need(services.sdkProgressSummariesEnabled,"getSdkAgentProgressSummariesEnabled");
    if(services.sdkProgressSummariesEnabled()){
      need(services.emitMissionProgress,"emitTaskProgress");
      Json event={{"taskId",id},{"description",summary},
        {"startTime",field(*captured,"startTime")},
        {"totalTokens",field(*captured,"tokenCount")},
        {"toolUses",field(*captured,"toolUseCount")},{"summary",summary}};
      const Json& gadgetUseId=field(*captured,"toolUseId");
      if(!gadgetUseId.is_null())event["toolUseId"]=gadgetUseId;
      services.emitMissionProgress(event);
    }
  }
}
void completeBatMission(const Json& result,BatMissionStateServices& services,
  const BatMessageRefs* refs){
  const Json& idValue=field(result,"agentId");
  if(!idValue.is_string())throw std::invalid_argument("Missing agentId in result");
  const std::string id=idValue.get<std::string>();
  updateMission(id,services,[&](const Json& mission)->std::optional<Json>{
    if(field(mission,"status")!="running")return std::nullopt;
    need(services.unregisterCleanup,"unregisterCleanup");
    services.unregisterCleanup(id);
    Json next=mission;materialize(next,refs);
    next["result"]=result;
    terminalFields(next,"completed",services);return next;
  });
  need(services.evictMissionOutput,"evictTaskOutput");services.evictMissionOutput(id);
}
void failBatMission(const std::string& id,const std::string& error,
  BatMissionStateServices& services,const BatMessageRefs* refs){
  updateMission(id,services,[&](const Json& mission)->std::optional<Json>{
    if(field(mission,"status")!="running")return std::nullopt;
    need(services.unregisterCleanup,"unregisterCleanup");
    services.unregisterCleanup(id);
    Json next=mission;materialize(next,refs);
    next["error"]=error;
    terminalFields(next,"failed",services);return next;
  });
  need(services.evictMissionOutput,"evictTaskOutput");services.evictMissionOutput(id);
}
} // namespace batbox::bats
