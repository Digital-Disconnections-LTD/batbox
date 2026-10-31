#include "shell/FormatDuration.hpp"
#include "runtime/JsonValue.hpp"
#include <charconv>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace batbox::shell {
namespace {
std::string fixedOne(double x){
  if(!std::isfinite(x)||std::abs(x)>=1e21)return value::numberToString(x);
  if(x==0)return "0.0";
  const bool negative=std::signbit(x);x=std::abs(x);
  if(x<0.05)return (negative?"-":"")+std::string("0.0");
  if(x>=9007199254740992.0){
    char data[256];const auto [end,error]=std::to_chars(data,data+sizeof(data),x,
      std::chars_format::fixed,1);
    if(error!=std::errc{})throw std::runtime_error("fixed conversion failed");
    return (negative?"-":"")+std::string(data,end);
  }
  const long double scaled=static_cast<long double>(x)*10.0L;
  const long double floorValue=std::floor(scaled);
  const auto rounded=static_cast<std::uint64_t>(floorValue+(scaled-floorValue>=0.5L?1:0));
  return (negative?"-":"")+std::to_string(rounded/10)+"."+std::to_string(rounded%10);
}
std::string str(double n){return value::numberToString(n);}
}
std::string formatDuration(double ms,bool hideTrailingZeros,bool mostSignificantOnly){
  if(ms<60000){
    if(ms==0)return "0s";
    if(ms<1)return fixedOne(ms/1000)+"s";
    return str(std::floor(ms/1000))+"s";
  }
  double days=std::floor(ms/86400000);
  double hours=std::floor(std::fmod(ms,86400000)/3600000);
  double minutes=std::floor(std::fmod(ms,3600000)/60000);
  double seconds=value::round(std::fmod(ms,60000)/1000);
  if(seconds==60){seconds=0;minutes++;}
  if(minutes==60){minutes=0;hours++;}
  if(hours==24){hours=0;days++;}
  if(mostSignificantOnly){
    if(days>0)return str(days)+"d";
    if(hours>0)return str(hours)+"h";
    if(minutes>0)return str(minutes)+"m";
    return str(seconds)+"s";
  }
  if(days>0){
    if(hideTrailingZeros&&hours==0&&minutes==0)return str(days)+"d";
    if(hideTrailingZeros&&minutes==0)return str(days)+"d "+str(hours)+"h";
    return str(days)+"d "+str(hours)+"h "+str(minutes)+"m";
  }
  if(hours>0){
    if(hideTrailingZeros&&minutes==0&&seconds==0)return str(hours)+"h";
    if(hideTrailingZeros&&seconds==0)return str(hours)+"h "+str(minutes)+"m";
    return str(hours)+"h "+str(minutes)+"m "+str(seconds)+"s";
  }
  if(minutes>0){
    if(hideTrailingZeros&&seconds==0)return str(minutes)+"m";
    return str(minutes)+"m "+str(seconds)+"s";
  }
  return str(seconds)+"s";
}
} // namespace batbox::shell
