#include "bootstrap/FlightCron.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using batbox::Json;
std::shared_ptr<batbox::bootstrap::FlightCronMission> readMission(const Json& data){
  auto mission=std::make_shared<batbox::bootstrap::FlightCronMission>();
  mission->id=data.at("id").get<std::string>();
  mission->cron=data.at("cron").get<std::string>();
  mission->poetry=data.at("prompt").get<std::string>();
  mission->createdAt=data.at("createdAt").get<double>();
  if(data.contains("recurring"))mission->recurring=data["recurring"].get<bool>();
  if(data.contains("agentId"))mission->batId=data["agentId"].get<std::string>();
  return mission;
}
Json writeMission(const batbox::bootstrap::FlightCronMission& mission){
  Json out={{"id",mission.id},{"cron",mission.cron},{"prompt",mission.poetry},
    {"createdAt",mission.createdAt}};
  if(mission.recurring)out["recurring"]=*mission.recurring;
  if(mission.batId)out["agentId"]=*mission.batId;
  return out;
}
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    bootstrap::FlightCronState state;
    auto previous=bootstrap::getFlightCronMissions(state);
    std::shared_ptr<bootstrap::FlightCronMission> lastMission;
    Json trace=Json::array();
    if(test.contains("ops"))for(const auto& op:test["ops"]){
      const auto kind=op["kind"].get<std::string>();
      if(kind=="add"||kind=="directAdd"){
        lastMission=readMission(op["task"]);
        if(kind=="add")bootstrap::addFlightCronMission(state,lastMission);
        else bootstrap::getFlightCronMissions(state)->push_back(lastMission);
        trace.push_back({{"sameArray",
          bootstrap::getFlightCronMissions(state)==previous}});
      }else if(kind=="mutateTask"){
        lastMission->poetry=op["prompt"].get<std::string>();
        bool sameMission=false;
        for(const auto& mission:*bootstrap::getFlightCronMissions(state))
          if(mission==lastMission)sameMission=true;
        trace.push_back({{"sameTask",sameMission}});
      }else if(kind=="remove"){
        std::vector<std::string> ids;
        for(const auto& id:op["ids"])ids.push_back(id.get<std::string>());
        const auto removed=bootstrap::removeFlightCronMissions(state,ids);
        trace.push_back({{"removed",removed},{"sameArray",
          bootstrap::getFlightCronMissions(state)==previous}});
      }else throw std::runtime_error("Unknown cron operation");
      previous=bootstrap::getFlightCronMissions(state);
    }
    Json missions=Json::array();
    for(const auto& mission:*bootstrap::getFlightCronMissions(state))
      missions.push_back(writeMission(*mission));
    std::cout<<value::stringify(Json{{"tasks",missions},{"trace",trace}})<<'\n';
  }
}
