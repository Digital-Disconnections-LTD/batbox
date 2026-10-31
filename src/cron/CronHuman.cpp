#include "cron/CronHuman.hpp"
#include "runtime/JsonValue.hpp"
#include <regex>
#include <stdexcept>
#include <vector>

namespace batbox::cron {
namespace {
const std::regex digits(R"(^\d+$)");
const std::regex oneDigit(R"(^\d$)");
const std::regex step(R"(^\*/(\d+)$)");
std::vector<std::string> fields(const std::string& value){
  std::vector<std::string> out;
  const auto text=value::trim(value);
  const auto spaceWidth=[&](std::size_t i)->std::size_t{
    const auto c=static_cast<unsigned char>(text[i]);
    if((c>=9&&c<=13)||c==0x20)return 1;
    if(i+1<text.size()&&c==0xc2&&
       static_cast<unsigned char>(text[i+1])==0xa0)return 2;
    if(i+2<text.size()){
      const auto b=static_cast<unsigned char>(text[i+1]);
      const auto d=static_cast<unsigned char>(text[i+2]);
      if(c==0xe1&&b==0x9a&&d==0x80)return 3;
      if(c==0xe2&&b==0x80&&
         ((d>=0x80&&d<=0x8a)||d==0xa8||d==0xa9||d==0xaf))return 3;
      if(c==0xe2&&b==0x81&&d==0x9f)return 3;
      if(c==0xe3&&b==0x80&&d==0x80)return 3;
      if(c==0xef&&b==0xbb&&d==0xbf)return 3;
    }
    return 0;
  };
  for(std::size_t i=0;i<text.size();){
    const auto space=spaceWidth(i);
    if(space){i+=space;continue;}
    const auto begin=i;
    while(i<text.size()&&spaceWidth(i)==0)++i;
    out.push_back(text.substr(begin,i-begin));
  }
  return out;
}
std::string padMinute(double minute){
  auto text=value::numberToString(minute);
  if(text.size()<2)text.insert(text.begin(),2-text.size(),'0');
  return text;
}
}
std::string cronToHuman(const std::string& expression,bool utc,
  const CronHumanServices& services){
  if(!services.formatLocalTime||!services.formatUtcTimeAsLocal||
     !services.utcDayOfWeekAsLocal)
    throw std::invalid_argument("Cron human time bindings required");
  const auto parts=fields(expression);
  if(parts.size()!=5)return expression;
  const auto& minute=parts[0];const auto& hour=parts[1];
  const auto& dayOfMonth=parts[2];const auto& month=parts[3];
  const auto& dayOfWeek=parts[4];
  std::smatch match;
  if(std::regex_match(minute,match,step)&&hour=="*"&&
     dayOfMonth=="*"&&month=="*"&&dayOfWeek=="*"){
    const auto n=value::parseInt10(match[1].str());
    return n==1?"Every minute":
      "Every "+value::numberToString(n)+" minutes";
  }
  if(std::regex_match(minute,digits)&&hour=="*"&&
     dayOfMonth=="*"&&month=="*"&&dayOfWeek=="*"){
    const auto m=value::parseInt10(minute);
    return m==0?"Every hour":"Every hour at :"+padMinute(m);
  }
  if(std::regex_match(hour,match,step)&&
     std::regex_match(minute,digits)&&dayOfMonth=="*"&&
     month=="*"&&dayOfWeek=="*"){
    const auto n=value::parseInt10(match[1].str());
    const auto m=value::parseInt10(minute);
    const auto suffix=m==0?"": " at :"+padMinute(m);
    return n==1?"Every hour"+suffix:
      "Every "+value::numberToString(n)+" hours"+suffix;
  }
  if(!std::regex_match(minute,digits)||
     !std::regex_match(hour,digits))return expression;
  const auto m=value::parseInt10(minute),h=value::parseInt10(hour);
  // Coerce the numeric input before converting it to a date.
  const auto format=[&]{return utc?
    services.formatUtcTimeAsLocal(m,h):
    services.formatLocalTime(m,h);};
  if(dayOfMonth=="*"&&month=="*"&&dayOfWeek=="*")
    return "Every day at "+format();
  if(dayOfMonth=="*"&&month=="*"&&
     std::regex_match(dayOfWeek,oneDigit)){
    const auto day=static_cast<int>(value::parseInt10(dayOfWeek))%7;
    const auto index=utc?services.utcDayOfWeekAsLocal(day,h,m):day;
    static const std::vector<std::string> names={"Sunday","Monday",
      "Tuesday","Wednesday","Thursday","Friday","Saturday"};
    if(index>=0&&index<7)return "Every "+names[index]+" at "+format();
  }
  if(dayOfMonth=="*"&&month=="*"&&dayOfWeek=="1-5")
    return "Weekdays at "+format();
  return expression;
}
}
