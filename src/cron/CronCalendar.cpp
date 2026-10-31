#include "cron/CronExpression.hpp"
#include "runtime/IndigenousTime.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <optional>
#include <vector>

namespace batbox::cron {
namespace {
bool contains(const std::vector<int>& values,int value){
  return std::binary_search(values.begin(),values.end(),value);
}
bool sameCivil(const std::tm& a,const std::tm& b){
  return a.tm_year==b.tm_year&&a.tm_mon==b.tm_mon&&
    a.tm_mday==b.tm_mday&&a.tm_hour==b.tm_hour&&
    a.tm_min==b.tm_min&&a.tm_sec==b.tm_sec;
}
std::optional<std::tm> localCivil(std::time_t seconds){
  std::tm result{};
  if(!runtime::localtimeIndigenous(seconds,result))return std::nullopt;
  return result;
}
std::optional<double> compatibleLocalMillis(std::tm requested){
  // Normalize out-of-range civil fields independently of timezone, as Date
  // setters do before resolving local-time DST overlap or gap.
  requested.tm_isdst=0;
  const auto civilSeconds=runtime::timegmUtc(&requested);
  std::tm normalized{};
  if(!runtime::gmtimeUtc(civilSeconds,normalized))return std::nullopt;
  std::optional<std::time_t> exact;
  struct GapCandidate {std::time_t epoch;std::int64_t civilDelta;};
  std::optional<GapCandidate> forward;
  for(int dst:{0,1,-1}){
    auto candidate=normalized;
    candidate.tm_isdst=dst;
    const auto epoch=::mktime(&candidate);
    const auto local=localCivil(epoch);
    if(!local)continue;
    if(sameCivil(*local,normalized)){
      if(!exact||epoch<*exact)exact=epoch;
      continue;
    }
    auto localAsUtc=*local;localAsUtc.tm_isdst=0;
    const auto localCivilSeconds=runtime::timegmUtc(&localAsUtc);
    const auto delta=static_cast<std::int64_t>(localCivilSeconds)-
      static_cast<std::int64_t>(civilSeconds);
    if(delta>=0&&(!forward||delta<forward->civilDelta||
        (delta==forward->civilDelta&&epoch<forward->epoch)))
      forward=GapCandidate{epoch,delta};
  }
  if(exact)return static_cast<double>(*exact)*1000;
  if(forward)return static_cast<double>(forward->epoch)*1000;
  return std::nullopt;
}
class LocalDate {
  double millis_;
public:
  explicit LocalDate(double millis):millis_(std::trunc(millis)){}
  std::optional<std::tm> parts()const{
    if(!std::isfinite(millis_)||std::abs(millis_)>8640000000000000.0)
      return std::nullopt;
    const auto seconds=static_cast<std::time_t>(
      std::floor(millis_/1000));
    return localCivil(seconds);
  }
  double millis()const{return millis_;}
  bool setSecondsZero(){
    auto value=parts();if(!value)return false;
    value->tm_sec=0;return set(*value);
  }
  bool setMinutes(int minute){
    auto value=parts();if(!value)return false;
    value->tm_min=minute;return set(*value);
  }
  bool setMonth(int month,int day){
    auto value=parts();if(!value)return false;
    value->tm_mon=month;value->tm_mday=day;return set(*value);
  }
  bool setDate(int day){
    auto value=parts();if(!value)return false;
    value->tm_mday=day;return set(*value);
  }
  bool setHours(int hour,int minute,int second){
    auto value=parts();if(!value)return false;
    value->tm_hour=hour;value->tm_min=minute;value->tm_sec=second;
    return set(*value);
  }
private:
  bool set(std::tm requested){
    const auto next=compatibleLocalMillis(requested);
    if(!next)return false;
    millis_=*next;return true;
  }
};
}
std::optional<double> computeNextCronRun(const CronFields& fields,
  double fromMs){
  LocalDate t(fromMs);
  auto current=t.parts();if(!current)return std::nullopt;
  if(!t.setSecondsZero())return std::nullopt;
  current=t.parts();if(!current||!t.setMinutes(current->tm_min+1))
    return std::nullopt;
  const bool domWild=fields.dayOfMonth.size()==31;
  const bool dowWild=fields.dayOfWeek.size()==7;
  constexpr int maxIter=366*24*60;
  for(int i=0;i<maxIter;++i){
    current=t.parts();if(!current)return std::nullopt;
    const int month=current->tm_mon+1;
    if(!contains(fields.month,month)){
      if(!t.setMonth(current->tm_mon+1,1)||!t.setHours(0,0,0))
        return std::nullopt;
      continue;
    }
    const int dom=current->tm_mday,dow=current->tm_wday;
    const bool dayMatches=domWild&&dowWild?true:
      domWild?contains(fields.dayOfWeek,dow):
      dowWild?contains(fields.dayOfMonth,dom):
      contains(fields.dayOfMonth,dom)||contains(fields.dayOfWeek,dow);
    if(!dayMatches){
      if(!t.setDate(current->tm_mday+1)||!t.setHours(0,0,0))
        return std::nullopt;
      continue;
    }
    if(!contains(fields.hour,current->tm_hour)){
      if(!t.setHours(current->tm_hour+1,0,0))return std::nullopt;
      continue;
    }
    if(!contains(fields.minute,current->tm_min)){
      if(!t.setMinutes(current->tm_min+1))return std::nullopt;
      continue;
    }
    return t.millis();
  }
  return std::nullopt;
}
}
