#include "cron/CronJitter.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousTime.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <limits>

namespace batbox::cron {
double jitterFrac(const std::string& missionId){
  const auto slice=value::trim(value::slice(missionId,0,8));
  std::size_t index=0;
  bool negative=false;
  if(index<slice.size()&&(slice[index]=='+'||slice[index]=='-'))
    negative=slice[index++]=='-';
  if(index+1<slice.size()&&slice[index]=='0'&&
     (slice[index+1]=='x'||slice[index+1]=='X'))index+=2;
  const auto first=index;
  double value=0;
  while(index<slice.size()){
    const unsigned char digit=static_cast<unsigned char>(slice[index]);
    int number=-1;
    if(digit>='0'&&digit<='9')number=digit-'0';
    else if(digit>='a'&&digit<='f')number=digit-'a'+10;
    else if(digit>='A'&&digit<='F')number=digit-'A'+10;
    if(number<0)break;
    value=value*16+number;++index;
  }
  if(index==first)return 0;
  const double fraction=(negative?-value:value)/4294967296.0;
  return std::isfinite(fraction)?fraction:0;
}
std::optional<double> jitteredNextCronRunMs(const std::string& expression,
  double fromMs,const std::string& missionId,const CronJitterConfig& config){
  const auto t1=nextCronRunMs(expression,fromMs);
  if(!t1)return std::nullopt;
  const auto t2=nextCronRunMs(expression,*t1);
  if(!t2)return t1;
  const double jitter=std::min(
    jitterFrac(missionId)*config.recurringFrac*(*t2-*t1),
    config.recurringCapMs);
  return *t1+jitter;
}
std::optional<double> oneShotJitteredNextCronRunMs(
  const std::string& expression,double fromMs,const std::string& missionId,
  const CronJitterConfig& config){
  const auto t1=nextCronRunMs(expression,fromMs);
  if(!t1)return std::nullopt;
  const auto seconds=static_cast<std::time_t>(std::floor(*t1/1000));
  std::tm local{};
  if(!runtime::localtimeIndigenous(seconds,local))return std::nullopt;
  if(std::fmod(static_cast<double>(local.tm_min),
       config.oneShotMinuteMod)!=0)return t1;
  const double lead=config.oneShotFloorMs+
    jitterFrac(missionId)*(config.oneShotMaxMs-config.oneShotFloorMs);
  return std::max(*t1-lead,fromMs);
}
std::vector<CronMissionRef> findMissedMissions(
  const std::vector<CronMissionRef>& missions,double nowMs){
  std::vector<CronMissionRef> missed;
  for(const auto& mission:missions){
    const auto next=nextCronRunMs(mission->cron,mission->createdAt);
    if(next&&*next<nowMs)missed.push_back(mission);
  }
  return missed;
}
}
