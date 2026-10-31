#include "cron/config/CronConfiguration.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::cron::config {
namespace {
Json defaultJitter(){
  Json value=Json::object();
  value["recurringFrac"]=defaultCronJitterConfig.recurringFrac;
  value["recurringCapMs"]=defaultCronJitterConfig.recurringCapMs;
  value["oneShotMaxMs"]=defaultCronJitterConfig.oneShotMaxMs;
  value["oneShotFloorMs"]=defaultCronJitterConfig.oneShotFloorMs;
  value["oneShotMinuteMod"]=defaultCronJitterConfig.oneShotMinuteMod;
  value["recurringMaxAgeMs"]=defaultCronJitterConfig.recurringMaxAgeMs;
  return value;
}
bool number(const Json& raw,const char* field,double minimum,
  double maximum,bool integer,double& output){
  if(!raw.contains(field)||!raw[field].is_number())return false;
  const double value=raw[field].get<double>();
  if(!std::isfinite(value)||value<minimum||value>maximum||
     (integer&&std::floor(value)!=value))return false;
  output=value;return true;
}
}
CronJitterConfig getCronJitterConfig(const Services& services){
  if(!services.getStashedConfiguration)
    throw std::invalid_argument("Cron local configuration binding required");
  const auto raw=services.getStashedConfiguration(services.jitterKey,
    defaultJitter(),jitterRefreshMs);
  if(!raw.is_object())return defaultCronJitterConfig;
  CronJitterConfig parsed;
  if(!number(raw,"recurringFrac",0,1,false,parsed.recurringFrac)||
     !number(raw,"recurringCapMs",0,1800000,true,
       parsed.recurringCapMs)||
     !number(raw,"oneShotMaxMs",0,1800000,true,
       parsed.oneShotMaxMs)||
     !number(raw,"oneShotFloorMs",0,1800000,true,
       parsed.oneShotFloorMs)||
     !number(raw,"oneShotMinuteMod",1,60,true,
       parsed.oneShotMinuteMod))return defaultCronJitterConfig;
  if(raw.contains("recurringMaxAgeMs")&&
      !number(raw,"recurringMaxAgeMs",0,2592000000.0,true,
        parsed.recurringMaxAgeMs))return defaultCronJitterConfig;
  if(parsed.oneShotFloorMs>parsed.oneShotMaxMs)
    return defaultCronJitterConfig;
  return parsed;
}
bool isCronEnabled(const Services& services){
  if(!services.batTriggersBuilt||!services.isEnvironmentTruthy||
     !services.getStashedConfiguration)
    throw std::invalid_argument("Cron gate bindings required");
  return services.batTriggersBuilt()&&
    !services.isEnvironmentTruthy(services.disableEnvironment)&&
    value::truthy(services.getStashedConfiguration(services.enabledKey,
      true,gateRefreshMs));
}
bool isDurableCronEnabled(const Services& services){
  if(!services.getStashedConfiguration)
    throw std::invalid_argument("Cron local configuration binding required");
  return value::truthy(services.getStashedConfiguration(services.durableKey,
    true,gateRefreshMs));
}
}
