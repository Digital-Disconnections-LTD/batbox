#include "cron/CronHuman.hpp"
#include "cron/CronScheduler.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <memory>
#include <string>

using batbox::Json;
int main(){
  const auto services=batbox::cron::indigenousCronHumanServices();
  for(std::string line;std::getline(std::cin,line);){
    const auto test=batbox::value::parse(line);
    std::string result;
    if(test.at("kind")=="human"){
      result=batbox::cron::cronToHuman(
        test.at("cron").get<std::string>(),test.value("utc",false),
        services);
    }else{
      std::vector<batbox::cron::CronMissionRef> missions;
      for(const auto& value:test.at("tasks")){
        auto mission=std::make_shared<batbox::cron::CronMission>();
        mission->cron=value.at("cron").get<std::string>();
        mission->poetry=value.at("prompt").get<std::string>();
        mission->createdAt=value.at("createdAt").get<double>();
        missions.push_back(std::move(mission));
      }
      result=batbox::cron::buildMissedMissionNotification(missions,
        [&services](const std::string& expression){
          return batbox::cron::cronToHuman(expression,false,services);},
        batbox::cron::indigenousLocalDateTime);
    }
    std::cout<<batbox::value::stringify(result)<<'\n';
  }
}
