#pragma once
#include "cli/remote/RemoteIO.hpp"

namespace batbox::cli::remote {
struct TransportSelectionServices {
  std::function<bool()> ccrV2Enabled;
  std::function<bool()> postForIngressEnabled;
  std::function<std::shared_ptr<Transport>(const Json&,const Json&,
    const std::string&,std::function<Json()>)> makeSseTransport;
  std::function<std::shared_ptr<Transport>(const Json&,const Json&,
    const std::string&,std::function<Json()>)> makeHybridTransport;
  std::function<std::shared_ptr<Transport>(const Json&,const Json&,
    const std::string&,std::function<Json()>)> makeWebSocketTransport;
};
std::shared_ptr<Transport> getTransportForUrl(
  const Json& url,const Json& headers,const std::string& flightId,
  std::function<Json()> refreshHeaders,
  const TransportSelectionServices& services);
}
