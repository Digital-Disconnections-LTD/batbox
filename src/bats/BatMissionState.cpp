#include "bats/BatMissionState.hpp"
#include "bats/ResumeFilters.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>

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
namespace {
bool settledStatus(const Json& mission){
  const auto& status=field(mission,"status");
  return status=="completed"||status=="failed"||status=="killed";
}
std::string clippedText(const Json& value,std::size_t limit){
  if(!value.is_string())return {};
  const auto& text=value.get_ref<const std::string&>();
  return value::stringLength(text)>limit?value::slice(text,0,limit-1)+"…":text;
}
Json summarizedRecord(const Json& mission){
  Json summary=Json::object();
  for(const char* key:{"id","agentId","type","status","description","name","model",
      "agentType","startTime","endTime","toolUseId","outputFile","outputOffset",
      "notified","retrieved","isBackgrounded","evictAfter","inputResumeError"})
    if(mission.contains(key))summary[key]=mission[key];
  if(mission.contains("prompt"))summary["prompt"]=clippedText(mission["prompt"],2000);
  if(mission.contains("error"))summary["error"]=clippedText(mission["error"],400);
  const Json& progress=field(mission,"progress");
  if(progress.is_object()){
    Json kept=Json::object();
    for(const char* key:{"toolUseCount","tokenCount","lastText","summary"})
      if(progress.contains(key))kept[key]=progress[key];
    const Json& activity=field(progress,"lastActivity");
    if(activity.is_object()){
      Json last=Json::object();
      for(const char* key:{"toolName","activityDescription"})
        if(activity.contains(key))last[key]=activity[key];
      kept["lastActivity"]=std::move(last);
    }
    summary["progress"]=std::move(kept);
  }
  const Json& result=field(mission,"result");
  if(result.is_object()){
    Json kept=Json::object();
    for(const char* key:{"totalTokens","totalDurationMs","totalToolUseCount"})
      if(result.contains(key))kept[key]=result[key];
    summary["result"]=std::move(kept);
  }
  summary["pendingMessages"]=Json::array();
  summary["retain"]=false;summary["diskLoaded"]=false;summary["summarized"]=true;
  return summary;
}
}
Json savedBatRecord(const std::string& id,const std::string& description,
  const std::string& agentType,const std::string& poetry,std::int64_t startTime,
  std::int64_t endTime,std::size_t gadgetUses,const Json& lastAssistant,
  const std::string& outputFile){
  const Json& body=field(lastAssistant,"message");
  const Json& usage=field(body,"usage");
  double tokens=0;
  for(const char* key:{"input_tokens","cache_creation_input_tokens",
      "cache_read_input_tokens","output_tokens"})
    if(field(usage,key).is_number())tokens+=field(usage,key).get<double>();
  Json progress={{"toolUseCount",gadgetUses},{"tokenCount",tokens}};
  if(const auto line=lastAssistantTextLine(lastAssistant);!line.empty())
    progress["lastText"]=line;
  Json record={{"id",id},{"agentId",id},{"type","local_agent"},{"status","completed"},
    {"description",description.empty()?std::string("Saved Bat"):description},
    {"agentType",agentType.empty()?std::string("general-purpose"):agentType},
    {"prompt",clippedText(Json(poetry),2000)},{"startTime",startTime},
    {"endTime",endTime},{"outputFile",outputFile},{"outputOffset",0},
    {"notified",true},{"retrieved",true},{"isBackgrounded",true},
    {"pendingMessages",Json::array()},{"retain",false},{"diskLoaded",false},
    {"savedOnDisk",true},{"progress",std::move(progress)}};
  if(field(body,"model").is_string())record["model"]=body["model"];
  return record;
}
SettledBatSweep sweepSettledBatMissions(const Json& missions,std::int64_t nowMs,
  std::size_t keepSettled,const std::function<bool(const std::string&)>& busy){
  SettledBatSweep sweep;
  if(!missions.is_object())return sweep;
  std::optional<Json> next;
  const auto edit=[&]()->Json&{if(!next)next=missions;return *next;};
  struct Settled {std::string id;std::int64_t at;};
  std::vector<Settled> settled;
  for(auto it=missions.begin();it!=missions.end();++it){
    const auto& mission=it.value();
    if(!isPanelBatMission(mission)||!settledStatus(mission)||
       value::truthy(field(mission,"retain"))||(busy&&busy(it.key())))continue;
    const Json& evict=field(mission,"evictAfter");
    if(evict.is_number()){
      const auto due=static_cast<std::int64_t>(evict.get<double>());
      if(due>nowMs){
        sweep.nextDueMs=sweep.nextDueMs?std::min(*sweep.nextDueMs,due):due;
        continue;
      }
      if(due==0){edit().erase(it.key());++sweep.removed;continue;}
      if(!value::truthy(field(mission,"summarized"))){
        edit()[it.key()]=summarizedRecord(mission);++sweep.summarized;
      }
    }
    const Json& end=field(mission,"endTime");
    const Json& start=field(mission,"startTime");
    settled.push_back({it.key(),end.is_number()?static_cast<std::int64_t>(end.get<double>()):
      start.is_number()?static_cast<std::int64_t>(start.get<double>()):0});
  }
  if(settled.size()>keepSettled){
    std::stable_sort(settled.begin(),settled.end(),
      [](const Settled& a,const Settled& b){return a.at>b.at;});
    for(auto it=settled.begin()+static_cast<std::ptrdiff_t>(keepSettled);
        it!=settled.end();++it){edit().erase(it->id);++sweep.removed;}
  }
  sweep.missions=std::move(next);
  return sweep;
}
} // namespace batbox::bats
