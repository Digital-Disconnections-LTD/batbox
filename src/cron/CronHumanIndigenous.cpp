#include "cron/CronHuman.hpp"
#include "runtime/Platform.hpp"
#include "runtime/IndigenousTime.hpp"
#include <cmath>
#include <ctime>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace batbox::cron {
namespace {
std::tm local(std::time_t timestamp){
  std::tm out{};
  if(!runtime::localtimeIndigenous(timestamp,out))
    throw std::runtime_error("local cron date conversion failed");
  return out;
}
std::tm utc(std::time_t timestamp){
  std::tm out{};
  if(!runtime::gmtimeUtc(timestamp,out))
    throw std::runtime_error("UTC cron date conversion failed");
  return out;
}
std::string timeText(const std::tm& value,bool withZone){
  const auto hour=value.tm_hour%12==0?12:value.tm_hour%12;
  std::string text=std::to_string(hour)+":"+
    (value.tm_min<10?"0":"")+std::to_string(value.tm_min)+
    (value.tm_hour<12?" AM":" PM");
  if(withZone){
    char zone[64]{};
    if(std::strftime(zone,sizeof(zone),"%Z",&value)>0){
      text+=" ";text+=zone;
    }
  }
  return text;
}
bool assignTime(std::tm& date,double minute,double hour){
  if(!std::isfinite(minute)||!std::isfinite(hour))return false;
  const auto total=std::trunc(hour)*60+std::trunc(minute);
  if(!std::isfinite(total)||std::abs(total)>144000000000.0)
    return false;
  const auto days=std::floor(total/1440);
  const auto remainder=total-days*1440;
  if(days<std::numeric_limits<int>::min()+31.0||
     days>std::numeric_limits<int>::max()-31.0)return false;
  date.tm_mday+=static_cast<int>(days);
  date.tm_hour=static_cast<int>(std::floor(remainder/60));
  date.tm_min=static_cast<int>(std::fmod(remainder,60));
  date.tm_sec=0;
  return true;
}
bool validDate(std::time_t stamp){
  return std::abs(static_cast<double>(stamp)*1000)<=8.64e15;
}
std::optional<std::time_t> todayUtcAt(double hour,double minute){
  auto today=utc(static_cast<std::time_t>(platform::nowMillis()/1000));
  if(!assignTime(today,minute,hour))return std::nullopt;
  const auto stamp=runtime::timegmUtc(&today);
  if(!validDate(stamp))return std::nullopt;
  return stamp;
}
}
CronHumanServices indigenousCronHumanServices(){
  return {
    [](double minute,double hour){
      std::tm date{};date.tm_year=100;date.tm_mday=1;
      date.tm_isdst=-1;
      if(!assignTime(date,minute,hour))return std::string("Invalid Date");
      const auto timestamp=::mktime(&date);
      if(!validDate(timestamp))return std::string("Invalid Date");
      return timeText(local(timestamp),false);
    },
    [](double minute,double hour){
      const auto stamp=todayUtcAt(hour,minute);
      return stamp?timeText(local(*stamp),true):std::string("Invalid Date");
    },
    [](int day,double hour,double minute){
      auto today=utc(static_cast<std::time_t>(platform::nowMillis()/1000));
      const auto offset=(day-today.tm_wday+7)%7;
      today.tm_mday+=offset;
      if(!assignTime(today,minute,hour))return -1;
      const auto stamp=runtime::timegmUtc(&today);
      return validDate(stamp)?local(stamp).tm_wday:-1;
    }
  };
}
std::string indigenousLocalDateTime(double epochMs){
  const auto date=local(static_cast<std::time_t>(std::floor(epochMs/1000)));
  const auto hour=date.tm_hour%12==0?12:date.tm_hour%12;
  std::string text=std::to_string(date.tm_mon+1)+"/"+
    std::to_string(date.tm_mday)+"/"+
    std::to_string(date.tm_year+1900)+", "+
    std::to_string(hour)+":"+
    (date.tm_min<10?"0":"")+std::to_string(date.tm_min)+":"+
    (date.tm_sec<10?"0":"")+std::to_string(date.tm_sec)+
    (date.tm_hour<12?" AM":" PM");
  return text;
}
}
