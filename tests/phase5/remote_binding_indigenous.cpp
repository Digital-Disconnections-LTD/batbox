#include "cli/remote/IndigenousRemoteBinding.hpp"
#include "runtime/Url.hpp"
#include <iostream>

using namespace batbox;
using namespace batbox::cli::remote;
int main(){
  event_loop::Loop loop;
  Services app;
  app.getFlightIngressAuthToken=[]()->std::optional<std::string>{
    return "local-token";
  };
  app.environment=[](const std::string& key)->std::optional<std::string>{
    if(key=="BATBOX_WORKER_EPOCH")return "7";
    if(key=="NO_PROXY")return "localhost";
    return {};
  };
  IndigenousRemoteTransportConfig config;
  config.sse.getAuthHeaders=[]{return Json{{"Authorization",
    "Bearer local-token"}};};
  config.sse.userBat=[]{return "batbox-native-binding";};
  config.sse.logDebug=[](const std::string&,const Json&){};
  config.sse.protocolVersionHeaderName="batbox-version";
  config.sse.protocolVersionHeaderValue="2026-10-29";
  config.ccr.registerFlightActivity=[](auto){};
  config.ccr.unregisterFlightActivity=[]{};
  config.ccr.onEpochMismatch=[]{};
  bool ccr=true,hybrid=false;
  config.ccrV2Enabled=[&]{return ccr;};
  config.postForIngressEnabled=[&]{return hybrid;};
  bindIndigenousRemoteTransports(app,loop,config);
  const auto url=runtime::parseUrl(
    "wss://localhost/v1/code/sessions/session-1");
  const auto httpUrl=runtime::parseUrl(
    "https://localhost/v1/code/sessions/session-1");
  if(!url||!httpUrl)throw std::runtime_error("URL parse failure");
  auto refresh=[] {return Json::object();};
  auto sse=app.getTransportForUrl(*httpUrl,Json::object(),
    "session-1",refresh);
  if(!sse||!sse->isSse)throw std::runtime_error("SSE selection failed");
  auto worker=app.createCcrClient(*sse,*httpUrl);
  if(!worker||!worker->initialize||!worker->writeEvent||
     !worker->readInternalEvents||!worker->reportDelivery)
    throw std::runtime_error("CCR binding failed");
  worker->close();
  ccr=false;hybrid=true;
  auto mixed=app.getTransportForUrl(*url,Json::object(),
    "session-1",refresh);
  if(!mixed||mixed->isSse||!mixed->write)
    throw std::runtime_error("Hybrid selection failed");
  mixed->close();
  hybrid=false;
  auto websocket=app.getTransportForUrl(*url,Json::object(),
    "session-1",refresh);
  if(!websocket||websocket->isSse||!websocket->connect)
    throw std::runtime_error("WebSocket selection failed");
  websocket->close();
  std::cout<<"native remote transport binding passed\n";
}
