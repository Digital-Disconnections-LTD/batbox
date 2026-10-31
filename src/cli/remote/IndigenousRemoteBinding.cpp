#include "cli/remote/IndigenousRemoteBinding.hpp"
#include "cli/remote/HybridTransport.hpp"
#include "cli/remote/UrlSearch.hpp"
#include "cli/remote/FlightToken.hpp"
#include "cli/remote/WebSocketProxy.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/Platform.hpp"
#include <chrono>
#include <random>
#include <stdexcept>

namespace batbox::cli::remote {
namespace {
struct SelectionState {
  std::shared_ptr<SSETransport> sse;
};
}
void bindIndigenousRemoteTransports(Services& app,event_loop::Loop& loop,
    IndigenousRemoteTransportConfig config){
  if(!config.ccrV2Enabled||!config.postForIngressEnabled||
     !app.getFlightIngressAuthToken)
    throw std::invalid_argument("Remote transport binding imports required");
  const auto state=std::make_shared<SelectionState>();
  if(!config.ccr.getAuthHeaders)
    config.ccr.getAuthHeaders=config.sse.getAuthHeaders;
  if(!config.ccr.getFlightIngressAuthToken)
    config.ccr.getFlightIngressAuthToken=app.getFlightIngressAuthToken;
  if(!config.ccr.decodeJwtExpiry){
    const auto prefix=config.flightTokenPrefix;
    config.ccr.decodeJwtExpiry=[prefix](const std::string& token){
      return decodeFlightJwtExpiry(token,prefix);
    };
  }
  if(!config.ccr.getEnvironmentWorkerEpoch){
    const auto environment=app.environment;
    const auto key=config.environmentWorkerEpochKey;
    if(environment)config.ccr.getEnvironmentWorkerEpoch=
      [environment,key]{return environment(key);};
  }
  if(!config.ccr.userBat)config.ccr.userBat=config.sse.userBat;
  if(!config.ccr.logDebug)config.ccr.logDebug=config.sse.logDebug;
  if(!config.ccr.randomUUID)config.ccr.randomUUID=platform::randomUUID;
  if(!config.ccr.nowMs)config.ccr.nowMs=[]{
    return static_cast<double>(std::chrono::duration_cast<
      std::chrono::milliseconds>(std::chrono::system_clock::now()
        .time_since_epoch()).count());
  };
  if(!config.ccr.randomUnit)config.ccr.randomUnit=[]{
    thread_local std::mt19937_64 generator(std::random_device{}());
    return std::generate_canonical<double,53>(generator);
  };
  if(!config.websocket.getProxyUrl&&app.environment){
    const auto environment=app.environment;
    config.websocket.getProxyUrl=[environment](const std::string& url){
      return getWebSocketProxyUrl(url,environment);
    };
  }
  if(!config.websocket.getTlsOptions){
    const auto network=config.sse.network;
    config.websocket.getTlsOptions=[network]()->std::optional<Json>{
      if(network.tlsCertificatePem.empty()&&
         network.tlsPrivateKeyPem.empty()&&network.tlsCaPem.empty()&&
         network.tlsKeyPassphrase.empty())return {};
      Json options=Json::object();
      if(!network.tlsCertificatePem.empty())
        options["cert"]=network.tlsCertificatePem;
      if(!network.tlsPrivateKeyPem.empty())
        options["key"]=network.tlsPrivateKeyPem;
      if(!network.tlsCaPem.empty())options["ca"]=network.tlsCaPem;
      if(!network.tlsKeyPassphrase.empty())
        options["passphrase"]=network.tlsKeyPassphrase;
      return options;
    };
  }
  if(!config.websocket.nowMs)config.websocket.nowMs=config.ccr.nowMs;
  if(!config.websocket.randomUnit)
    config.websocket.randomUnit=config.ccr.randomUnit;
  if(!config.websocket.logDebug)
    config.websocket.logDebug=config.sse.logDebug;
  if(!config.websocket.registerFlightActivity)
    config.websocket.registerFlightActivity=
      config.ccr.registerFlightActivity;
  if(!config.websocket.unregisterFlightActivity)
    config.websocket.unregisterFlightActivity=
      config.ccr.unregisterFlightActivity;
  if(config.ccr.protocolVersionHeaderName.empty()){
    config.ccr.protocolVersionHeaderName=
      config.sse.protocolVersionHeaderName;
    config.ccr.protocolVersionHeaderValue=
      config.sse.protocolVersionHeaderValue;
  }
  if(!config.ccr.http)
    config.ccr.http=makeIndigenousCcrHttp(loop,config.ccrNetwork);
  const auto token=app.getFlightIngressAuthToken;
  app.getTransportForUrl=[state,&loop,config,token](const Json& url,
      const Json& headers,const std::string& flightId,
      std::function<Json()> refreshHeaders){
    TransportSelectionServices selection;
    selection.ccrV2Enabled=config.ccrV2Enabled;
    selection.postForIngressEnabled=config.postForIngressEnabled;
    selection.makeSseTransport=[state,&loop,config](const Json& target,
        const Json& incoming,const std::string& id,
        std::function<Json()> refresh){
      auto services=makeIndigenousSseServices(loop,config.sse);
      state->sse=std::make_shared<SSETransport>(loop,target,incoming,id,
        std::move(refresh),std::nullopt,std::move(services));
      return state->sse->asTransport();
    };
    selection.makeWebSocketTransport=[&loop,config](const Json& target,
        const Json& incoming,const std::string& id,
        std::function<Json()> refresh){
      return makeWebSocketTransport(loop,target,incoming,id,
        std::move(refresh),
        makeIndigenousWebSocketServices(loop,config.websocket));
    };
    selection.makeHybridTransport=[&loop,config,token](const Json& target,
        const Json& incoming,const std::string& id,
        std::function<Json()> refresh){
      auto postConfig=config.sse;
      postConfig.network.requestTimeout=std::chrono::seconds(15);
      auto post=makeIndigenousSseServices(loop,std::move(postConfig));
      HybridServices services;
      services.websocket=makeIndigenousWebSocketServices(loop,config.websocket);
      services.getFlightIngressAuthToken=token;
      services.post=post.post;
      services.logDebug=config.sse.logDebug;
      services.randomUnit=post.randomUnit;
      services.jsonStringify=post.jsonStringify;
      services.maxConsecutiveFailures=
        config.hybridMaxConsecutiveFailures;
      services.onBatchDropped=config.onHybridBatchDropped;
      return makeHybridTransport(loop,target,incoming,id,
        std::move(refresh),std::move(services));
    };
    return getTransportForUrl(url,headers,flightId,
      std::move(refreshHeaders),selection);
  };
  app.createCcrClient=[state,&loop,config](Transport&,const Json& url){
    if(!state->sse)
      throw std::invalid_argument("CCR requires the selected SSE stream");
    return makeCcrClient(loop,state->sse,url,config.ccr);
  };
}
}
