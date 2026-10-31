#include "diagnostics/LogDates.hpp"
#include "runtime/IndigenousTime.hpp"
#include <cmath>
#include <cstdio>
#include <ctime>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace batbox::diagnostics {
namespace {
std::vector<long long> digitGroups(const std::string& text){
  std::vector<long long> groups;
  for(std::size_t index=0;index<text.size();){
    while(index<text.size()&&(text[index]<'0'||text[index]>'9'))++index;
    if(index==text.size())break;
    long long value=0;
    while(index<text.size()&&text[index]>='0'&&text[index]<='9'){
      const auto digit=text[index++]-'0';
      if(value>(std::numeric_limits<long long>::max()-digit)/10)
        return {};
      value=value*10+digit;
    }
    groups.push_back(value);
  }
  return groups;
}
}

double parseLogISOString(const std::string& text){
  if(text.empty()||text.front()<'0'||text.front()>'9')
    return std::numeric_limits<double>::quiet_NaN();
  const auto fields=digitGroups(text);
  if(fields.size()<7)return std::numeric_limits<double>::quiet_NaN();
  for(std::size_t i=0;i<7;++i)
    if(fields[i]>1'000'000'000)return std::numeric_limits<double>::quiet_NaN();
  long long year=fields[0];
  if(year>=0&&year<=99)year+=1900; // Date.UTC's two-digit-year rule.
  std::tm utc{};
  utc.tm_year=static_cast<int>(year-1900);
  utc.tm_mon=static_cast<int>(fields[1]-1);
  utc.tm_mday=static_cast<int>(fields[2]);
  utc.tm_hour=static_cast<int>(fields[3]);
  utc.tm_min=static_cast<int>(fields[4]);
  utc.tm_sec=static_cast<int>(fields[5]);
  const auto seconds=runtime::timegmUtc(&utc);
  const double milliseconds=static_cast<double>(seconds)*1000+
    static_cast<double>(fields[6]);
  if(!std::isfinite(milliseconds)||std::abs(milliseconds)>8.64e15)
    return std::numeric_limits<double>::quiet_NaN();
  return std::trunc(milliseconds);
}

std::string logDateISOString(double milliseconds){
  if(!std::isfinite(milliseconds)||std::abs(milliseconds)>8.64e15)
    throw std::range_error("Invalid time value");
  const long long total=static_cast<long long>(std::trunc(milliseconds));
  long long seconds=total/1000;
  long long fraction=total%1000;
  if(fraction<0){fraction+=1000;--seconds;}
  const auto epoch=static_cast<std::time_t>(seconds);
  std::tm utc{};
  if(!runtime::gmtimeUtc(epoch,utc))throw std::range_error("Invalid time value");
  const long long year=static_cast<long long>(utc.tm_year)+1900;
  char result[64];
  if(year>=0&&year<=9999)
    std::snprintf(result,sizeof(result),"%04lld-%02d-%02dT%02d:%02d:%02d.%03lldZ",
      year,utc.tm_mon+1,utc.tm_mday,utc.tm_hour,utc.tm_min,utc.tm_sec,fraction);
  else
    std::snprintf(result,sizeof(result),"%c%06lld-%02d-%02dT%02d:%02d:%02d.%03lldZ",
      year<0?'-':'+',year<0?-year:year,utc.tm_mon+1,utc.tm_mday,
      utc.tm_hour,utc.tm_min,utc.tm_sec,fraction);
  return result;
}

std::string dateToFilename(double milliseconds){
  auto filename=logDateISOString(milliseconds);
  for(auto& character:filename)
    if(character==':'||character=='.')character='-';
  return filename;
}
}
