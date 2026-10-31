#include "transport/SdkRetry.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace batbox::transport {
namespace {
std::string header(const Json& headers,const std::string& name){
  if(!headers.is_object())return {};
  for(auto item=headers.begin();item!=headers.end();++item){
    auto key=item.key();
    std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){
      return c>='A'&&c<='Z'?c+('a'-'A'):c;});
    if(key==name&&item.value().is_string())return item.value().get<std::string>();
  }
  return {};
}
}
bool sdkShouldRetry(long status,const Json& headers,SdkRetryAuth& auth){
  if(status==401&&auth.invalidateTokenStash&&auth.usedTokenStash&&!auth.didRefreshFor401){
    auth.didRefreshFor401=true;auth.invalidateTokenStash();return true;
  }
  const auto value=header(headers,"x-should-retry");
  if(value=="true")return true;
  if(value=="false")return false;
  return status==408||status==409||status==429||status>=500;
}
double sdkDefaultRetryDelay(int remaining,int maximum,double random){
  return std::min(0.5*std::pow(2.,maximum-remaining),8.)*(1-random*0.25)*1000;
}
double sdkRetryDelay(const Json& headers,int remaining,int maximum,
    const SdkRetryClock& clock){
  double delay=std::numeric_limits<double>::quiet_NaN();
  const auto millis=header(headers,"retry-after-ms");
  if(!millis.empty()){
    const auto value=value::parseFloat(millis);
    if(!std::isnan(value))delay=value;
  }
  const auto seconds=header(headers,"retry-after");
  if(!seconds.empty()&&(std::isnan(delay)||delay==0)){
    const auto value=value::parseFloat(seconds);
    if(!std::isnan(value))delay=value*1000;
    else{
      if(!clock.parseDate||!clock.nowMillis)throw std::invalid_argument("Retry date clock required");
      delay=clock.parseDate(seconds)-clock.nowMillis();
    }
  }
  if(!(delay>0&&delay<=2147483647.)){
    if(!clock.random)throw std::invalid_argument("Retry random source required");
    delay=sdkDefaultRetryDelay(remaining,maximum,clock.random());
  }
  return delay;
}
} // namespace batbox::transport
