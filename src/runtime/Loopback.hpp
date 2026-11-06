#pragma once
#include <string>

namespace batbox::runtime {
// Whether url names this machine: localhost, *.localhost, 127.0.0.0/8, ::1 or
// 0.0.0.0. Requests to these never go through a proxy.
inline bool isLoopbackUrl(const std::string& url){
  const auto scheme=url.find("://");
  auto host=url.substr(scheme==std::string::npos?0:scheme+3);
  host=host.substr(0,host.find_first_of("/?#"));
  if(const auto at=host.rfind('@');at!=std::string::npos)host=host.substr(at+1);
  if(host.starts_with('['))host=host.substr(1,host.find(']')-1);
  else if(const auto colon=host.find(':');colon!=std::string::npos)host=host.substr(0,colon);
  for(auto& c:host)if(c>='A'&&c<='Z')c=static_cast<char>(c+('a'-'A'));
  return host=="localhost"||host.ends_with(".localhost")||host.starts_with("127.")||
    host=="::1"||host=="0.0.0.0";
}
} // namespace batbox::runtime
