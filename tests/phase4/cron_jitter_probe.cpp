#include "cron/CronJitter.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
batbox::cron::CronJitterConfig config(const batbox::Json& op){
  batbox::cron::CronJitterConfig result;
  if(!op.contains("cfg")||op["cfg"].is_null())return result;
  const auto& cfg=op["cfg"];
  result.recurringFrac=cfg["recurringFrac"].get<double>();
  result.recurringCapMs=cfg["recurringCapMs"].get<double>();
  result.oneShotMaxMs=cfg["oneShotMaxMs"].get<double>();
  result.oneShotFloorMs=cfg["oneShotFloorMs"].get<double>();
  result.oneShotMinuteMod=cfg["oneShotMinuteMod"].get<double>();
  result.recurringMaxAgeMs=cfg["recurringMaxAgeMs"].get<double>();
  return result;
}
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json result=Json::array();
    if(test.contains("ops"))for(const auto& op:test["ops"]){
      const auto kind=op["kind"].get<std::string>();
      Json value=nullptr;
      if(kind=="frac")value=cron::jitterFrac(op["id"].get<std::string>());
      else if(kind=="recurring"||kind=="oneshot"){
        const auto cfg=config(op);
        const auto expression=op["cron"].get<std::string>();
        const auto from=op["fromMs"].get<double>();
        const auto id=op["id"].get<std::string>();
        const auto next=kind=="recurring"?
          cron::jitteredNextCronRunMs(expression,from,id,cfg):
          cron::oneShotJitteredNextCronRunMs(expression,from,id,cfg);
        if(next)value=*next;
      }else if(kind=="missed"){
        std::vector<cron::CronMissionRef> missions;
        for(const auto& item:op["tasks"]){
          auto mission=std::make_shared<cron::CronMission>();
          mission->id=item["id"].get<std::string>();
          mission->cron=item["cron"].get<std::string>();
          mission->createdAt=item["createdAt"].get<double>();
          missions.push_back(mission);
        }
        const auto missed=cron::findMissedMissions(missions,
          op["nowMs"].get<double>());
        Json ids=Json::array();bool same=true;
        for(const auto& mission:missed){
          ids.push_back(mission->id);
          if(std::find(missions.begin(),missions.end(),mission)==missions.end())same=false;
        }
        value={{"ids",ids},{"sameReferences",same}};
      }else throw std::runtime_error("Unknown jitter operation");
      result.push_back(value);
    }
    std::cout<<value::stringify(result)<<'\n';
  }
}
