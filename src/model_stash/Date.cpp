#include "model_stash/Date.hpp"
#include "runtime/IndigenousTime.hpp"
#include <cmath>
#include <ctime>
#include <limits>
#include <regex>
#include <string>

namespace batbox::model_stash {
double indigenousMessageTimestampMs(const std::string& timestamp){
  static const std::regex iso(
    R"(^([0-9]{4}|[+-][0-9]{6})-([0-9]{2})-([0-9]{2})[Tt]([0-9]{2}):([0-9]{2})(?::([0-9]{2})(?:\.([0-9]+))?)?([Zz]|[+-][0-9]{2}:?[0-9]{2})$)");
  std::smatch parts;
  if(!std::regex_match(timestamp,parts,iso))
    return std::numeric_limits<double>::quiet_NaN();
  const auto year=std::stoi(parts[1]),month=std::stoi(parts[2]);
  const auto day=std::stoi(parts[3]),hour=std::stoi(parts[4]);
  const auto minute=std::stoi(parts[5]);
  const auto second=parts[6].matched?std::stoi(parts[6]):0;
  if(parts[1]=="-000000"||month<1||month>12||day<1||day>31||hour>24||
      minute>59||second>59||(hour==24&&(minute!=0||second!=0)))
    return std::numeric_limits<double>::quiet_NaN();
  std::tm fields{};
  fields.tm_year=year-1900;fields.tm_mon=month-1;fields.tm_mday=day;
  fields.tm_hour=hour;fields.tm_min=minute;fields.tm_sec=second;
  const auto epoch=runtime::timegmUtc(&fields);
  int millis=0;
  if(parts[7].matched){
    auto digits=parts[7].str();
    if(hour==24&&digits.find_first_not_of('0')!=std::string::npos)
      return std::numeric_limits<double>::quiet_NaN();
    digits=digits.substr(0,3);
    while(digits.size()<3)digits+='0';
    millis=std::stoi(digits);
  }
  if(hour==24&&millis!=0)return std::numeric_limits<double>::quiet_NaN();
  int offset=0;
  if(parts[8]!="Z"&&parts[8]!="z"){
    const auto zone=parts[8].str();
    const int hours=std::stoi(zone.substr(1,2));
    const int minutes=std::stoi(zone.substr(zone[3]==':'?4:3,2));
    if(hours>23||minutes>59)return std::numeric_limits<double>::quiet_NaN();
    offset=(hours*60+minutes)*60;
    if(zone[0]=='-')offset=-offset;
  }
  const auto result=static_cast<double>(epoch-offset)*1000.0+millis;
  return std::abs(result)>8640000000000000.0?
    std::numeric_limits<double>::quiet_NaN():result;
}
}
