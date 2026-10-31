#include "bat_context/PartnerMissions.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace batbox;
namespace {
bat_context::PartnerMissionState stateFrom(const Json& input){
  bat_context::PartnerMissionState result;
  for(auto it=input.begin();it!=input.end();++it){
    const auto& data=it.value();
    auto mission=std::make_shared<bat_context::PartnerMission>();
    mission->type=value::property(data,"type").get<std::string>();
    mission->status=value::property(data,"status").get<std::string>();
    mission->isIdle=value::property(data,"isIdle");
    const auto seed=data.value("seedCallbacks",0);
    for(int index=0;index<seed;++index)
      mission->onIdleCallbacks.push_back([]{});
    result.missions.emplace_back(it.key(),std::move(mission));
  }
  return result;
}
Json summary(const bat_context::PartnerMissionState& state){
  Json output=Json::object();
  for(const auto& [id,mission]:state.missions)
    output[id]={{"type",mission->type},{"status",mission->status},
      {"isIdle",mission->isIdle},{"callbacks",mission->onIdleCallbacks.size()}};
  return output;
}
Json run(const Json& input){
  const auto initial=stateFrom(input.at("initial"));
  const auto& currentData=value::property(input,"current");
  auto current=stateFrom(currentData.is_object()?currentData:input.at("initial"));
  const Json before={{"active",bat_context::hasActiveInProcessPartners(initial)},
    {"working",bat_context::hasWorkingInProcessPartners(initial)}};
  event_loop::Loop loop;
  bool settled=false;
  std::optional<std::string> error;
  int setterCalls=0;
  bool cloned=false;
  const auto setter=[&](std::function<bat_context::PartnerMissionState(
      const bat_context::PartnerMissionState&)> update){
    ++setterCalls;
    if(input.value("setterThrows",false))
      throw std::runtime_error("setter failed");
    current=update(current);
    cloned=true;
  };
  auto waiting=bat_context::waitForPartnersToBecomeIdle(
    loop,setter,initial);
  waiting.then([&](const event_loop::Unit*,const std::exception_ptr& cause){
    if(cause){try{std::rethrow_exception(cause);}
      catch(const std::exception& ex){error=ex.what();}}
    else settled=true;
  });
  loop.runReady();
  const Json beforeFire={{"settled",settled},
    {"error",error?Json(*error):Json()},
    {"setterCalls",setterCalls},{"tasks",summary(current)},
    {"tasksCloned",cloned}};
  Json afterFire=Json::array();
  const auto& fire=value::property(input,"fire");
  if(fire.is_array())for(const auto& idValue:fire){
    const auto id=idValue.get<std::string>();
    for(const auto& [missionId,mission]:current.missions){
      if(missionId==id){
        const auto callbacks=mission->onIdleCallbacks;
        for(const auto& callback:callbacks)callback();
        break;
      }
    }
    loop.runReady();
    afterFire.push_back(Json{{"id",id},{"settled",settled},
      {"error",error?Json(*error):Json()}});
  }
  return Json{{"before",before},{"beforeFire",beforeFire},
    {"afterFire",afterFire}};
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line))
    std::cout<<value::stringify(run(value::parse(line)))<<'\n';
}
