#include "application_mission_ui/BatMissionBridge.hpp"
#include "application/Async.hpp"
#include "runtime/JsonValue.hpp"
#include "shell/TailFile.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::application_mission_ui {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string string(const Json& value){
  return value.is_string()?value.get<std::string>():"";
}
long long integer(const Json& value){
  return value.is_number()?static_cast<long long>(value.get<double>()):0;
}
std::optional<std::string> optionalString(const Json& value){
  if(value.is_string())return value.get<std::string>();
  return std::nullopt;
}
std::string resultText(const Json& result){
  const auto& blocks=field(result,"content");
  std::string output;
  if(!blocks.is_array())return output;
  for(const auto& block:blocks){
    if(field(block,"type")!="text")continue;
    const auto text=string(field(block,"text"));
    if(text.empty())continue;
    if(!output.empty())output+='\n';
    output+=text;
  }
  return output;
}
shell_missions::MissionRecord recordFrom(const std::string& id,const Json& mission){
  shell_missions::MissionRecord record;
  record.id=id;record.type="local_agent";
  record.status=string(field(mission,"status"));
  record.description=string(field(mission,"description"));
  record.poetry=optionalString(field(mission,"prompt"));
  record.result=resultText(field(mission,"result"));
  record.error=optionalString(field(mission,"error"));
  record.gadgetUseId=optionalString(field(mission,"toolUseId"));
  record.batId=optionalString(field(mission,"agentId"));
  record.outputFilePath=optionalString(field(mission,"outputFile"));
  record.startTimeMs=integer(field(mission,"startTime"));
  if(field(mission,"endTime").is_number())record.endTimeMs=integer(field(mission,"endTime"));
  record.notified=field(mission,"notified")==true;
  record.isBackgrounded=field(mission,"isBackgrounded")!=false;
  return record;
}
std::string readOutputFile(const shell_missions::MissionRecord& mission){
  if(!mission.outputFilePath)return {};
  try{
    constexpr std::size_t maxBytes=8*1024*1024;
    const auto tail=shell::tailFile(*mission.outputFilePath,maxBytes);
    if(tail.bytesTotal>tail.bytesRead){
      const auto omitted=static_cast<long long>(std::llround(
        static_cast<double>(tail.bytesTotal-tail.bytesRead)/1024.0));
      return "["+std::to_string(omitted)+"KB of earlier output omitted]\n"+tail.content;
    }
    return tail.content;
  }catch(...){return {};}
}
}
BatMissionBridge::BatMissionBridge(event_loop::Loop& owner,
    std::shared_ptr<app_state::AppStateStore> store,
    std::shared_ptr<shell_missions::Roster> roster,
    std::function<void(const std::string&)> stopBatMission):
  owner_(owner),store_(std::move(store)),roster_(std::move(roster)),
  stopBatMission_(std::move(stopBatMission)){}
std::shared_ptr<BatMissionBridge> BatMissionBridge::create(event_loop::Loop& owner,
    std::shared_ptr<app_state::AppStateStore> store,
    std::shared_ptr<shell_missions::Roster> roster,
    std::function<void(const std::string&)> stopBatMission,
    std::function<void()> changed){
  if(!owner.isOwnerThread()||!store||!roster||!stopBatMission)
    throw std::invalid_argument("Agent task bridge requires owner, state, registry and stop");
  auto bridge=std::shared_ptr<BatMissionBridge>(new BatMissionBridge(owner,
    std::move(store),std::move(roster),std::move(stopBatMission)));
  bridge->changed_=std::move(changed);
  const std::weak_ptr<BatMissionBridge> weak=bridge;
  const auto dispatcher=owner.dispatcher();
  bridge->roster_->bindExternalServices(
    [weak,dispatcher](const shell_missions::MissionRecord& mission){
      application::onOwnerAndWait<void>(dispatcher,[weak,id=mission.id]{
        const auto self=weak.lock();
        if(!self)throw std::runtime_error("Agent task owner closed");
        self->stopBatMission_(id);
      });
    },readOutputFile,
    [weak,dispatcher](const shell_missions::MissionRecord& mission){
      application::onOwnerAndWait<void>(dispatcher,[weak,id=mission.id]{
        if(const auto self=weak.lock())self->markNotified(id);
      });
    });
  auto listener=std::make_shared<app_state::AppStateStore::Listener>(
    [weak]{if(auto self=weak.lock())self->synchronize();});
  bridge->unsubscribe_=bridge->store_->subscribe(std::move(listener));
  bridge->synchronize();
  return bridge;
}
BatMissionBridge::~BatMissionBridge(){if(unsubscribe_)unsubscribe_();}
void BatMissionBridge::synchronize(){
  if(!owner_.isOwnerThread())throw std::logic_error("Agent task sync requires owner");
  const auto state=store_->getState()->snapshot();
  const auto& missions=field(state,"tasks");
  std::set<std::string> current;
  if(missions.is_object())for(auto it=missions.begin();it!=missions.end();++it){
    if(field(it.value(),"type")!="local_agent")continue;
    current.insert(it.key());
    roster_->upsertExternal(recordFrom(it.key(),it.value()));
  }
  for(const auto& id:tracked_)if(!current.contains(id))roster_->eraseExternal(id);
  tracked_=std::move(current);
  if(changed_)changed_();
}
void BatMissionBridge::markNotified(const std::string& id){
  if(!owner_.isOwnerThread())throw std::logic_error("Agent task state requires owner");
  store_->setState([&id](const app_state::AppStateRef& previous){
    const auto state=previous->snapshot();
    const auto& missions=field(state,"tasks");
    const auto& mission=value::property(missions,id);
    if(mission.is_null()||field(mission,"notified")==true)return previous;
    auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
    Json updatedMissions=missions;
    updatedMissions[id]["notified"]=true;
    next->fields->set("tasks",updatedMissions);
    next->missions=previous->missions?
      std::make_shared<app_state::OrderedStringMap<value::ValueRef>>(*previous->missions):
      std::make_shared<app_state::OrderedStringMap<value::ValueRef>>();
    next->missions->set(id,std::make_shared<Json>(updatedMissions[id]));
    return next;
  });
}
} // namespace batbox::application_mission_ui
