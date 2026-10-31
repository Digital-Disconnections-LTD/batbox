#pragma once
#include <cerrno>
#include <cstdint>
#include <ctime>
#include <limits>

namespace batbox::runtime {
namespace indigenous_time {
inline std::int64_t floorDivide(std::int64_t value,std::int64_t divisor){
  const auto quotient=value/divisor;
  return quotient-(value%divisor<0);
}
inline std::int64_t civilDays(std::int64_t year,int month,int day){
  // March starts the leap-year cycle. Gregorian cycles contain 146097 days.
  year-=month<=2;
  const auto cycle=floorDivide(year,400);
  const auto cycleYear=year-cycle*400;
  const auto marchMonth=month+(month>2?-3:9);
  const auto yearDay=(153*marchMonth+2)/5+day-1;
  return cycle*146097+cycleYear*365+cycleYear/4-cycleYear/100+
    yearDay-719468;
}
inline bool utcParts(std::time_t time,std::tm& result){
  const auto seconds=static_cast<std::int64_t>(time);
  const auto days=floorDivide(seconds,86400);
  const auto daySeconds=seconds%86400+(seconds%86400<0?86400:0);
  const auto shifted=days+719468;
  const auto cycle=floorDivide(shifted,146097);
  const auto cycleDay=shifted-cycle*146097;
  const auto cycleYear=(cycleDay-cycleDay/1460+cycleDay/36524-
    cycleDay/146096)/365;
  const auto yearDay=cycleDay-(365*cycleYear+cycleYear/4-cycleYear/100);
  const auto marchMonth=(5*yearDay+2)/153;
  const int day=static_cast<int>(yearDay-(153*marchMonth+2)/5+1);
  const int month=static_cast<int>(marchMonth+(marchMonth<10?3:-9));
  const auto year=cycleYear+cycle*400+(month<=2);
  if(year-1900<std::numeric_limits<int>::min()||
     year-1900>std::numeric_limits<int>::max()){
    errno=EOVERFLOW;return false;
  }
  std::tm parts{};
  parts.tm_year=static_cast<int>(year-1900);parts.tm_mon=month-1;
  parts.tm_mday=day;parts.tm_hour=static_cast<int>(daySeconds/3600);
  parts.tm_min=static_cast<int>((daySeconds/60)%60);
  parts.tm_sec=static_cast<int>(daySeconds%60);
  parts.tm_wday=static_cast<int>(((days+4)%7+7)%7);
  parts.tm_yday=static_cast<int>(days-civilDays(year,1,1));
  parts.tm_isdst=0;result=parts;return true;
}
inline std::time_t utcSeconds(std::tm& value){
  const auto year=static_cast<std::int64_t>(value.tm_year)+1900+
    floorDivide(value.tm_mon,12);
  const auto month=static_cast<int>((value.tm_mon%12+12)%12)+1;
  const auto days=civilDays(year,month,1)+value.tm_mday-1;
  const auto seconds=days*86400+static_cast<std::int64_t>(value.tm_hour)*3600+
    static_cast<std::int64_t>(value.tm_min)*60+value.tm_sec;
  if(seconds<std::numeric_limits<std::time_t>::min()||
     seconds>std::numeric_limits<std::time_t>::max()){
    errno=EOVERFLOW;return static_cast<std::time_t>(-1);
  }
  const auto epoch=static_cast<std::time_t>(seconds);
  if(!utcParts(epoch,value))return static_cast<std::time_t>(-1);
  return epoch;
}
} // namespace indigenous_time
inline std::time_t timegmUtc(std::tm* value){
#if defined(_WIN32)
  // The Windows CRT only accepts UTC dates from 1970 through 3000.
  // Date conversion supports negative epochs and expanded years.
  return indigenous_time::utcSeconds(*value);
#else
  return ::timegm(value);
#endif
}
inline bool gmtimeUtc(const std::time_t& time,std::tm& result){
#if defined(_WIN32)
  return indigenous_time::utcParts(time,result);
#else
  return ::gmtime_r(&time,&result)!=nullptr;
#endif
}
inline bool localtimeIndigenous(const std::time_t& time,std::tm& result){
#if defined(_WIN32)
  return ::localtime_s(&result,&time)==0;
#else
  return ::localtime_r(&time,&result)!=nullptr;
#endif
}
}
