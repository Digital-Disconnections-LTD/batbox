#include "cron/print/HeadlessCron.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace batbox;
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const auto item=value::parse(line);
    Json events=Json::array();
    std::vector<cron::SchedulerOptions> schedulers;
    int nextUuid=0;
    cron::print::Services services;
    services.batTriggersBuilt=[&]{
      const bool value=item.at("feature").get<bool>();
      events.push_back(Json::array({"feature","AGENT_TRIGGERS",value}));
      return value;
    };
    services.schedulerModuleAvailable=item.at("module").get<bool>();
    if(item.at("gateAvailable")==true)
      services.cronEnabled=[&]{
        const bool value=item.at("enabled").get<bool>();
        events.push_back(Json::array({"gate",value}));
        return value;
      };
    services.getJitterConfig=[]{return cron::defaultCronJitterConfig;};
    services.createScheduler=[&](cron::SchedulerOptions options){
      const auto index=schedulers.size();schedulers.push_back(options);
      events.push_back(Json::array({"create",index}));
      return cron::use_scheduled::SchedulerHandle{
        [&,index]{events.push_back(Json::array({"start",index}));},
        [&,index]{events.push_back(Json::array({"stop",index}));},nullptr};
    };
    services.enqueue=[&](const Json& value){
      events.push_back(Json::array({"enqueue",value}));
    };
    services.randomUUID=[&]{return "uuid-"+std::to_string(++nextUuid);};
    services.run=[&]{events.push_back(Json::array({"run"}));};
    cron::print::HeadlessCron cron(std::move(services));
    cron.start();
    if(!schedulers.empty()){
      const auto current=schedulers[0];
      events.push_back(Json::array({"isLoading",current.isLoading()}));
      current.onFire("first");
      cron.setRunning(true);
      events.push_back(Json::array({"isLoading",current.isLoading()}));
      events.push_back(Json::array({"isKilled",current.isKilled()}));
      cron.closeInput();
      events.push_back(Json::array({"isLoading",current.isLoading()}));
      current.onFire("after close");
    }else cron.closeInput();
    std::cout<<value::stringify(events)<<'\n';
  }
}
