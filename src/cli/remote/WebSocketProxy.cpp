#include "cli/remote/WebSocketProxy.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/Url.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace batbox::cli::remote {
namespace {
bool space(std::uint32_t codepoint){
  return (codepoint>=9&&codepoint<=13)||codepoint==0x20||
    codepoint==0xa0||codepoint==0x1680||
    (codepoint>=0x2000&&codepoint<=0x200a)||
    codepoint==0x2028||codepoint==0x2029||codepoint==0x202f||
    codepoint==0x205f||codepoint==0x3000||codepoint==0xfeff;
}
std::vector<std::string> splitPatterns(const std::string& value){
  std::vector<std::string> out;
  std::size_t start=0;
  for(std::size_t at=0;at<value.size();){
    const auto begin=at;
    const auto first=static_cast<unsigned char>(value[at++]);
    std::uint32_t codepoint=first;
    int extra=0;
    if((first&0xe0)==0xc0){codepoint=first&0x1f;extra=1;}
    else if((first&0xf0)==0xe0){codepoint=first&0x0f;extra=2;}
    else if((first&0xf8)==0xf0){codepoint=first&0x07;extra=3;}
    while(extra--&&at<value.size()){
      codepoint=(codepoint<<6)|
        (static_cast<unsigned char>(value[at++])&0x3f);
    }
    if(codepoint==','||space(codepoint)){
      if(begin>start)out.push_back(value.substr(start,begin-start));
      start=at;
    }
  }
  if(start<value.size())out.push_back(value.substr(start));
  return out;
}
std::string lowerAscii(std::string value){
  std::transform(value.begin(),value.end(),value.begin(),
    [](unsigned char ch){return ch>='A'&&ch<='Z'?
      static_cast<char>(ch+32):static_cast<char>(ch);});
  return value;
}
std::optional<std::string> firstEnvironment(const ProxyEnvironment& env,
    std::initializer_list<const char*> names){
  for(const auto* key:names){
    const auto value=env(key);
    if(value&& !value->empty())return value;
  }
  return {};
}
}
bool shouldBypassWebSocketProxy(const std::string& url,
    const std::optional<std::string>& noProxy){
  if(!noProxy||noProxy->empty())return false;
  if(*noProxy=="*")return true;
  const auto parsed=runtime::parseUrl(url);
  if(!parsed)return false;
  const auto hostname=lowerAscii(value::property(*parsed,"hostname")
    .get<std::string>());
  auto port=value::property(*parsed,"port").get<std::string>();
  if(port.empty())port=value::property(*parsed,"protocol")=="https:"?
    "443":"80";
  const auto hostWithPort=hostname+":"+port;
  for(auto pattern:splitPatterns(*noProxy)){
    pattern=lowerAscii(value::trim(pattern));
    if(pattern.find(':')!=std::string::npos){
      if(hostWithPort==pattern)return true;
      continue;
    }
    if(pattern.starts_with('.')){
      if(hostname==pattern.substr(1)||hostname.ends_with(pattern))return true;
    }else if(hostname==pattern)return true;
  }
  return false;
}
std::optional<std::string> getWebSocketProxyUrl(
    const std::string& url,const ProxyEnvironment& environment){
  if(!environment)throw std::invalid_argument("Proxy environment required");
  const auto proxy=firstEnvironment(environment,
    {"https_proxy","HTTPS_PROXY","http_proxy","HTTP_PROXY"});
  if(!proxy)return {};
  const auto noProxy=firstEnvironment(environment,{"no_proxy","NO_PROXY"});
  if(shouldBypassWebSocketProxy(url,noProxy))return {};
  return proxy;
}
}
