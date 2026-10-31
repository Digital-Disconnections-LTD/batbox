#include "cron/use_scheduled/Flight.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace batbox;
namespace {
cron::CronMissionRef mission(const std::string& id,const std::string& poetry,
    const std::optional<std::string>& batId={}){
  auto result=std::make_shared<cron::CronMission>();
  result->id=id;result->poetry=poetry;result->batId=batId;
  return result;
}
}
int main(){
  event_loop::Loop loop;
  Json events=Json::array();
  std::vector<cron::SchedulerOptions> schedulers;
  bool gate=false;
  Json missions=Json::object();
  Json messages=Json::array();
  cron::use_scheduled::Services services;
  services.createScheduler=[&](cron::SchedulerOptions options){
    const auto index=schedulers.size();schedulers.push_back(options);
    events.push_back(Json::array({"create",index,options.assistantMode}));
    return cron::use_scheduled::SchedulerHandle{
      [&,index]{events.push_back(Json::array({"start",index}));},
      [&,index]{events.push_back(Json::array({"stop",index}));},nullptr};
  };
  services.cronEnabled=[&]{
    events.push_back(Json::array({"gate",gate}));return gate;
  };
  services.getJitterConfig=[]{return cron::defaultCronJitterConfig;};
  services.enqueuePendingNotification=[&](const Json& value){
    events.push_back(Json::array({"enqueue",value}));
  };
  services.getCurrentMissions=[&]{return missions;};
  services.findPartnerMissionByBatId=[&](const std::string& batId,
      const Json& values)->std::optional<cron::use_scheduled::PartnerMissionHandle>{
    events.push_back(Json::array({"find",batId}));
    for(const auto& value:values){
      if(value::property(value::property(value,"identity"),"agentId")==batId)
        return cron::use_scheduled::PartnerMissionHandle{
          value::property(value,"id").get<std::string>(),
          value::property(value,"status").get<std::string>()};
    }
    return std::nullopt;
  };
  services.isTerminalMissionStatus=[](const std::string& status){
    return status=="completed"||status=="failed"||status=="killed";
  };
  services.injectUserMessageToPartner=[&](const std::string& id,
      const std::string& poetry){
    events.push_back(Json::array({"inject",id,poetry}));
  };
  services.removeCronMissions=[&](const std::vector<std::string>& ids){
    events.push_back(Json::array({"remove",ids}));
  };
  services.logForDebugging=[&](const std::string& text){
    events.push_back(Json::array({"debug",text}));
  };
  services.createScheduledMissionFireMessage=[&](const std::string& text){
    events.push_back(Json::array({"messageFactory",text}));
    return Json{{"type","scheduled"},{"content",text}};
  };
  services.appendMessageAtomically=[&](const Json& message){
    messages.push_back(message);
    events.push_back(Json::array({"messages",messages}));
  };
  services.formatCronFireTime=[]{return std::string("Oct 29 6:24am");};
  cron::use_scheduled::Flight flight(loop,std::move(services));
  flight.render(true,false);flight.commit();
  gate=true;flight.render(false,false);flight.commit();
  flight.render(false,true);flight.commit();
  const auto current=schedulers[0];
  events.push_back(Json::array({"isLoading",current.isLoading()}));
  current.onFire("missed prompt");
  current.onFireMission(mission("lead","lead prompt"));
  missions={{"one",Json{{"id","task-1"},
    {"identity",Json{{"agentId","worker@team"}}},
    {"status","running"}}}};
  current.onFireMission(mission("worker","work","worker@team"));
  missions["one"]["status"]="killed";
  current.onFireMission(mission("orphan","work","worker@team"));
  missions=Json::object();
  current.onFireMission(mission("gone","work","absent@team"));
  gate=false;
  events.push_back(Json::array({"isKilled",current.isKilled()}));
  flight.render(true,true);flight.commit();
  events.push_back(Json::array({"isLoading",current.isLoading()}));
  gate=true;flight.render(false,false);flight.commit();
  events.push_back(Json::array({"isLoading",schedulers[1].isLoading()}));
  flight.unmount();
  std::cout<<value::stringify(events)<<'\n';
}
