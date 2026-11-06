#include "providers/RequestSettings.hpp"
#include "providers/Catalog.hpp"
#include "providers/cloud/CloudKinds.hpp"
#include <algorithm>
#include <array>
#include <cctype>

namespace batbox::providers {
namespace {
std::string lower(std::string text){
  std::transform(text.begin(),text.end(),text.begin(),
    [](unsigned char c){return static_cast<char>(std::tolower(c));});
  return text;
}
std::string hostOf(const std::string& url){
  const auto scheme=url.find("://");
  auto host=url.substr(scheme==std::string::npos?0:scheme+3);
  host=host.substr(0,host.find_first_of("/?#"));
  if(const auto at=host.rfind('@');at!=std::string::npos)host=host.substr(at+1);
  if(host.starts_with('[')){
    const auto close=host.find(']');
    host=host.substr(1,close==std::string::npos?std::string::npos:close-1);
  }else if(const auto colon=host.find(':');colon!=std::string::npos)host=host.substr(0,colon);
  while(host.ends_with('.'))host.pop_back();
  return lower(host);
}
std::optional<std::array<int,4>> ipv4(const std::string& host){
  std::array<int,4> parts{};
  std::size_t index=0,start=0;
  while(index<4){
    const auto end=host.find('.',start);
    const auto part=host.substr(start,end==std::string::npos?std::string::npos:end-start);
    if(part.empty()||part.size()>3||
        !std::all_of(part.begin(),part.end(),[](unsigned char c){return std::isdigit(c);}))
      return std::nullopt;
    parts[index++]=std::stoi(part);
    if(parts[index-1]>255)return std::nullopt;
    if(end==std::string::npos)break;
    start=end+1;
  }
  if(index!=4||host.find('.',start)!=std::string::npos)return std::nullopt;
  return parts;
}
bool endsWith(const std::string& host,const char* suffix){
  const std::string tail=suffix;
  return host.size()>tail.size()&&host.ends_with(tail);
}
}
bool localNetworkUrl(const std::string& url){
  const auto host=hostOf(url);
  if(host.empty())return false;
  if(const auto address=ipv4(host)){
    const auto& a=*address;
    return a[0]==127||a[0]==10||(a[0]==172&&a[1]>=16&&a[1]<=31)||
      (a[0]==192&&a[1]==168)||(a[0]==169&&a[1]==254)||
      (a[0]==100&&a[1]>=64&&a[1]<=127)||(a[0]==0&&a[1]==0&&a[2]==0&&a[3]==0);
  }
  if(host.find(':')!=std::string::npos){
    if(host=="::1"||host=="::")return true;
    return host.starts_with("fc")||host.starts_with("fd")||host.starts_with("fe8")||
      host.starts_with("fe9")||host.starts_with("fea")||host.starts_with("feb");
  }
  if(host=="localhost"||host.find('.')==std::string::npos)return true;
  for(const char* suffix:{".localhost",".local",".lan",".home.arpa",".internal",".ts.net"})
    if(endsWith(host,suffix))return true;
  return false;
}
RequestSettings requestSettings(const application::InferenceProfile& profile){
  const auto& limits=profile.limits;
  RequestSettings settings;
  if(limits.local)settings.local=*limits.local;
  else{
    const auto* preset=profile.preset.empty()?nullptr:bundledCatalog().find(profile.preset);
    settings.local=(preset&&preset->local)||
      (!cloud::isCloudKind(profile.provider)&&localNetworkUrl(profile.baseUrl));
  }
  using std::chrono::milliseconds;
  const auto pick=[](const std::optional<std::size_t>& value,milliseconds fallback){
    return value?milliseconds(static_cast<milliseconds::rep>(*value)):fallback;
  };
  // Local streams are bounded by inactivity alone: a long prompt and a slow
  // generation can legitimately run for hours.
  settings.requestTimeout=pick(limits.requestTimeoutMs,
    settings.local?milliseconds(0):std::chrono::hours(4));
  settings.streamIdleTimeout=pick(limits.streamIdleTimeoutMs,std::chrono::hours(2));
  settings.connectTimeout=pick(limits.connectTimeoutMs,std::chrono::seconds(10));
  settings.nonStreamingFallback=limits.nonStreamingFallback.value_or(!settings.local);
  settings.nonStreamingTimeout=pick(limits.nonStreamingTimeoutMs,
    settings.local?milliseconds(0):std::chrono::minutes(5));
  settings.maxRetries=limits.maxRetries;
  return settings;
}
} // namespace batbox::providers
