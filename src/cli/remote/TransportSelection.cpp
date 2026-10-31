#include "cli/remote/TransportSelection.hpp"
#include "runtime/Url.hpp"
#include <stdexcept>

namespace batbox::cli::remote {
namespace {
std::string field(const Json& url,const char* name){
  const auto& value=value::property(url,name);
  if(!value.is_string())throw std::invalid_argument("URL field required");
  return value.get<std::string>();
}
}
std::shared_ptr<Transport> getTransportForUrl(
    const Json& url,const Json& headers,const std::string& flightId,
    std::function<Json()> refreshHeaders,
    const TransportSelectionServices& services){
  if(!services.ccrV2Enabled||!services.postForIngressEnabled)
    throw std::invalid_argument("Transport feature imports required");
  const auto protocol=field(url,"protocol");
  if(services.ccrV2Enabled()){
    if(!services.makeSseTransport)
      throw std::invalid_argument("SSE transport import required");
    auto scheme=protocol;
    if(scheme=="wss:")scheme="https:";
    else if(scheme=="ws:")scheme="http:";
    auto pathname=field(url,"pathname");
    if(!pathname.empty()&&pathname.back()=='/')pathname.pop_back();
    const auto href=scheme+field(url,"href").substr(protocol.size());
    const auto parsed=runtime::parseUrl(href);
    if(!parsed)throw std::invalid_argument("Invalid stream URL");
    const auto target=field(*parsed,"origin")+pathname+
      "/worker/events/stream"+field(*parsed,"search")+
      field(*parsed,"hash");
    const auto sseUrl=runtime::parseUrl(target);
    if(!sseUrl)throw std::invalid_argument("Invalid SSE URL");
    return services.makeSseTransport(*sseUrl,headers,flightId,
      std::move(refreshHeaders));
  }
  if(protocol=="ws:"||protocol=="wss:"){
    if(services.postForIngressEnabled()){
      if(!services.makeHybridTransport)
        throw std::invalid_argument("Hybrid transport import required");
      return services.makeHybridTransport(url,headers,flightId,
        std::move(refreshHeaders));
    }
    if(!services.makeWebSocketTransport)
      throw std::invalid_argument("WebSocket transport import required");
    return services.makeWebSocketTransport(url,headers,flightId,
      std::move(refreshHeaders));
  }
  throw std::runtime_error("Unsupported protocol: "+protocol);
}
}
