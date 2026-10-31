#include "interactive/background/FlightMission.hpp"
#include <stdexcept>

namespace batbox::interactive::background {
namespace {
template<class T>void need(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Flight Mission binding required: ")+name);
}
const Json& field(const Json& value,const char* key){return value::property(value,key);}
Json missionSnapshot(const app_state::OrderedStringMap<value::ValueRef>& missions){
  Json result=Json::object();
  for(const auto& [id,mission]:missions.entries())if(mission)result[id]=*mission;
  return result;
}
std::optional<std::string> stringField(const Json& value,const char* key){
  const auto& item=field(value,key);
  return item.is_string()?std::optional<std::string>(item.get<std::string>()):
    std::nullopt;
}
std::string tag(const char* name,const std::string& value){
  return std::string("<")+name+">"+value+"</"+name+">";
}
std::shared_ptr<poetryEntries::MainThreadPoetryBat> defaultBat(){
  static auto bat=[](){
    auto value=std::make_shared<poetryEntries::MainThreadPoetryBat>();
    value->definition={{"agentType","main-session"},
      {"whenToUse","Main Flight query"},{"source","userSettings"}};
    value->getSystemPoetry=[](const execution::Context*){return Json("");};
    return value;
  }();
  return bat;
}
}
FlightMissions::FlightMissions(event_loop::Loop& owner,FlightMissionServices services):
  owner_(owner),services_(std::make_shared<FlightMissionServices>(std::move(services))){
  need(services_->store,"store");
}
std::string FlightMissions::generateId()const{
  need(services_->randomBytes8,"randomBytes8");
  constexpr char alphabet[]="0123456789abcdefghijklmnopqrstuvwxyz";
  std::string id="s";
  for(const auto byte:services_->randomBytes8())id+=alphabet[byte%36];
  return id;
}
void FlightMissions::setMission(const std::string& id,const Json& mission){
  services_->store->setState([&](const app_state::AppStateRef& previous){
    need(previous,"app state");need(previous->missions,"typed Missions");
    Json merged=mission;
    if(const auto* old=previous->missions->get(id);old&&*old&&
        (**old).is_object()&&(**old).contains("retain")){
      for(const char* key:{"retain","startTime","messages",
          "diskLoaded","pendingMessages"}){
        if((**old).contains(key))merged[key]=(**old).at(key);
        else merged.erase(key);
      }
    }
    auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
    next->missions=std::make_shared<app_state::OrderedStringMap<value::ValueRef>>(
      *previous->missions);
    next->missions->set(id,std::make_shared<Json>(std::move(merged)));
    next->fields->set("tasks",missionSnapshot(*next->missions));
    return next;
  });
}
void FlightMissions::updateMission(const std::string& id,
    const std::function<std::optional<Json>(const Json&)>& update){
  services_->store->setState([&](const app_state::AppStateRef& previous){
    need(previous,"app state");need(previous->missions,"typed Missions");
    const auto* existing=previous->missions->get(id);
    if(!existing||!*existing)return previous;
    auto updated=update(**existing);
    if(!updated)return previous;
    auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
    next->missions=std::make_shared<app_state::OrderedStringMap<value::ValueRef>>(
      *previous->missions);
    next->missions->set(id,std::make_shared<Json>(std::move(*updated)));
    next->fields->set("tasks",missionSnapshot(*next->missions));
    return next;
  });
}
void FlightMissions::removeMission(const std::string& id){
  services_->store->setState([&](const app_state::AppStateRef& previous){
    need(previous,"app state");need(previous->missions,"typed Missions");
    if(!previous->missions->get(id))return previous;
    auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
    next->missions=std::make_shared<app_state::OrderedStringMap<value::ValueRef>>(
      *previous->missions);
    next->missions->erase(id);
    next->fields->set("tasks",missionSnapshot(*next->missions));
    return next;
  });
  sidecars_.erase(id);
}
void FlightMissions::refreshMessageView(const std::string& id,
    const TurnMessagesRef& messages){
  const auto state=services_->store->getState();
  if(!state||!state->missions||!messages)return;
  Json current=Json::array();
  for(const auto& message:*messages)current.push_back(*message);
  if(const auto* mission=state->missions->get(id);mission&&*mission)
    (**mission)["messages"]=current;
  if(const auto missions=state->fieldRef("tasks");missions&&missions->is_object()&&
      missions->contains(id))(*missions)[id]["messages"]=std::move(current);
}
FlightMissionRef FlightMissions::find(const std::string& id)const{
  const auto found=sidecars_.find(id);
  return found==sidecars_.end()?FlightMissionRef{}:found->second;
}
FlightMissionRef FlightMissions::registerMainFlightMission(
    const std::string& description,const Json& batDefinition,
    std::shared_ptr<TurnAbort> existingAbort,
    std::shared_ptr<poetryEntries::MainThreadPoetryBat> selectedBat){
  if(!owner_.isOwnerThread())throw std::logic_error("Flight Mission owner required");
  auto& s=*services_;
  const std::string id=generateId();
  need(s.getBatTranscriptPath,"getAgentTranscriptPath");
  need(s.initMissionOutputAsSymlink,"initTaskOutputAsSymlink");
  s.initMissionOutputAsSymlink(id,s.getBatTranscriptPath(id));
  auto mission=std::make_shared<FlightMission>();
  mission->id=id;
  mission->abort=existingAbort?std::move(existingAbort):std::make_shared<TurnAbort>();
  if(!selectedBat){
    if(batDefinition.is_discarded())selectedBat=defaultBat();
    else{
      selectedBat=std::make_shared<poetryEntries::MainThreadPoetryBat>();
      selectedBat->definition=batDefinition;
    }
  }
  mission->selectedBat=std::move(selectedBat);
  need(s.registerCleanup,"registerCleanup");
  mission->unregisterCleanup=s.registerCleanup(
    [weak=weak_from_this(),id]{
      if(auto owner=weak.lock())owner->removeMission(id);
    });
  need(s.nowMillis,"nowMillis");need(s.getMissionOutputPath,"getTaskOutputPath");
  Json selected=mission->selectedBat->definition;
  Json state={{"id",id},{"type","local_agent"},{"status","running"},
    {"description",description},{"startTime",s.nowMillis()},
    {"outputFile",s.getMissionOutputPath(id)},{"outputOffset",0},
    {"notified",false},{"agentId",id},{"prompt",description},
    {"selectedAgent",selected},{"agentType","main-session"},
    {"abortController",Json::object()},{"retrieved",false},
    {"lastReportedToolCount",0},{"lastReportedTokenCount",0},
    {"isBackgrounded",true},{"pendingMessages",Json::array()},
    {"retain",false},{"diskLoaded",false}};
  if(s.logForDebugging)s.logForDebugging(
    "[LocalMainSessionTask] Registering Mission "+id+
    " with description: "+description);
  const bool replacement=s.store->getState()->missions->get(id)!=nullptr;
  if(replacement){
    if(const auto old=find(id);old)mission->messages=old->messages;
  }
  setMission(id,state);
  sidecars_[id]=mission;
  if(!replacement){
    need(s.enqueueSdkEvent,"enqueueSdkEvent");
    s.enqueueSdkEvent(Json{{"type","system"},{"subtype","task_started"},
      {"task_id",id},{"description",description},{"task_type","local_agent"},
      {"prompt",description}});
  }
  s.store->setState([&](const app_state::AppStateRef& previous){
    if(s.logForDebugging)s.logForDebugging(
      "[LocalMainSessionTask] After registration, Mission "+id+
      " exists in state: "+
      (previous->missions&&previous->missions->get(id)?"true":"false"));
    return previous;
  });
  return mission;
}
bool FlightMissions::isMainFlightMission(const Json& mission){
  return mission.is_object()&&field(mission,"type")=="local_agent"&&
    field(mission,"agentType")=="main-session";
}
void FlightMissions::completeMainFlightMission(const std::string& id,bool success){
  bool wasBackgrounded=true;
  std::optional<std::string> gadgetUseId;
  updateMission(id,[&](const Json& mission)->std::optional<Json>{
    if(field(mission,"status")!="running")return std::nullopt;
    wasBackgrounded=field(mission,"isBackgrounded").is_boolean()?
      field(mission,"isBackgrounded").get<bool>():true;
    gadgetUseId=stringField(mission,"toolUseId");
    if(auto sidecar=find(id);sidecar&&sidecar->unregisterCleanup)
      sidecar->unregisterCleanup();
    Json result=mission;
    result["status"]=success?"completed":"failed";
    need(services_->nowMillis,"nowMillis");result["endTime"]=services_->nowMillis();
    const auto sidecar=find(id);
    if(sidecar&&sidecar->messages&&!sidecar->messages->empty()){
      result["messages"]=Json::array({*sidecar->messages->back()});
      sidecar->messages=std::make_shared<TurnMessages>(
        TurnMessages{sidecar->messages->back()});
    }else result.erase("messages");
    return result;
  });
  need(services_->evictMissionOutput,"evictTaskOutput");
  services_->evictMissionOutput(id);
  if(wasBackgrounded){
    enqueueMainFlightNotification(id,"Background Flight",
      success?"completed":"failed",gadgetUseId);
  }else{
    updateMission(id,[](const Json& mission)->std::optional<Json>{
      Json result=mission;result["notified"]=true;return result;
    });
    need(services_->emitMissionTerminatedSdk,"emitTaskTerminatedSdk");
    services_->emitMissionTerminatedSdk(id,success?"completed":"failed",
      gadgetUseId,"Background Flight");
  }
}
void FlightMissions::enqueueMainFlightNotification(const std::string& id,
    const std::string& description,const std::string& status,
    const std::optional<std::string>& gadgetUseId){
  bool shouldEnqueue=false;
  updateMission(id,[&](const Json& mission)->std::optional<Json>{
    if(value::truthy(field(mission,"notified")))return std::nullopt;
    shouldEnqueue=true;
    Json result=mission;result["notified"]=true;return result;
  });
  if(!shouldEnqueue)return;
  const auto summary=status=="completed"?
    "Background session \""+description+"\" completed":
    "Background session \""+description+"\" failed";
  const auto gadgetLine=gadgetUseId&&!gadgetUseId->empty()?
    "\n"+tag("tool-use-id",*gadgetUseId):"";
  need(services_->getMissionOutputPath,"getTaskOutputPath");
  const auto message="<task-notification>\n"+tag("task-id",id)+gadgetLine+
    "\n"+tag("output-file",services_->getMissionOutputPath(id))+"\n"+
    tag("status",status)+"\n"+tag("summary",summary)+
    "\n</task-notification>";
  need(services_->enqueuePendingNotification,"enqueuePendingNotification");
  services_->enqueuePendingNotification(Json{{"value",message},
    {"mode","task-notification"}});
}
TurnMessagesRef FlightMissions::foregroundMainFlightMission(const std::string& id){
  TurnMessagesRef messages;
  services_->store->setState([&](const app_state::AppStateRef& previous){
    need(previous,"app state");need(previous->missions,"typed Missions");
    const auto* current=previous->missions->get(id);
    if(!current||!*current||field(**current,"type")!="local_agent")
      return previous;
    if(const auto sidecar=find(id);sidecar)messages=sidecar->messages;
    if(!messages){
      const auto& values=field(**current,"messages");
      if(values.is_array()){
        messages=std::make_shared<TurnMessages>();
        for(const auto& value:values)messages->push_back(std::make_shared<Json>(value));
      }
    }
    auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
    next->missions=std::make_shared<app_state::OrderedStringMap<value::ValueRef>>(
      *previous->missions);
    const auto prior=previous->fieldRef("foregroundedTaskId");
    if(prior&&prior->is_string()){
      const auto priorId=prior->get<std::string>();
      const auto* priorMission=previous->missions->get(priorId);
      if(priorId!=id&&priorMission&&*priorMission&&
          field(**priorMission,"type")=="local_agent"){
        Json restored=**priorMission;restored["isBackgrounded"]=true;
        next->missions->set(priorId,std::make_shared<Json>(std::move(restored)));
      }
    }
    Json active=**current;active["isBackgrounded"]=false;
    next->missions->set(id,std::make_shared<Json>(std::move(active)));
    next->fields->set("foregroundedTaskId",id);
    next->fields->set("tasks",missionSnapshot(*next->missions));
    return next;
  });
  return messages;
}
} // namespace batbox::interactive::background
