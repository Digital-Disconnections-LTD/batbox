#include "cron/config/CronConfiguration.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <string>

using batbox::Json;
using namespace batbox::cron::config;
namespace {
Json configJson(const batbox::cron::CronJitterConfig& value){
  Json output=Json::object();
  output["recurringFrac"]=value.recurringFrac;
  output["recurringCapMs"]=value.recurringCapMs;
  output["oneShotMaxMs"]=value.oneShotMaxMs;
  output["oneShotFloorMs"]=value.oneShotFloorMs;
  output["oneShotMinuteMod"]=value.oneShotMinuteMod;
  output["recurringMaxAgeMs"]=value.recurringMaxAgeMs;
  return output;
}
Json run(const Json& test){
  Json trace=Json::array();
  Services services;
  services.batTriggersBuilt=[&]{trace.push_back("feature:AGENT_TRIGGERS");
    return test.value("built",true);};
  services.isEnvironmentTruthy=[&](const auto& key){
    trace.push_back("env:"+key);
    if(!test.contains("disableEnv"))return false;
    const auto value=test.at("disableEnv");
    return value.is_string()&&value!=""&&value!="0"&&value!="false";
  };
  services.getStashedConfiguration=[&](const auto& key,
    const Json& fallback,double ttl){
    trace.push_back("config:"+key+":"+
      batbox::value::numberToString(ttl));
    const auto original=key=="batbox_cron_config"?
      "tengu_kairos_cron_config":key=="batbox_cron_enabled"?
        "tengu_kairos_cron":"tengu_kairos_cron_durable";
    return test.contains(original)?test.at(original):fallback;
  };
  Json results=Json::array();
  for(const auto& op:test.at("ops")){
    if(op=="jitter")results.push_back(configJson(getCronJitterConfig(services)));
    else if(op=="enabled")results.push_back(isCronEnabled(services));
    else results.push_back(isDurableCronEnabled(services));
  }
  Json output=Json::object();output["results"]=results;
  output["trace"]=trace;return output;
}
}
int main(){
  for(std::string line;std::getline(std::cin,line);)
    std::cout<<batbox::value::stringify(run(batbox::value::parse(line)))<<'\n';
}
