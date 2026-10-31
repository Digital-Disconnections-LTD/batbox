#include "client_factory/tls/TlsOptions.hpp"
#include <stdexcept>

namespace batbox::client_factory::tls {
namespace {
Json combine(const std::shared_ptr<const Json>& mtls,
    const std::shared_ptr<const std::vector<std::string>>& ca){
  Json config=mtls?*mtls:Json::object();
  if(ca)config["ca"]=*ca;
  return config;
}
}
NetworkOptions TlsOptions::getTLSFetchOptions(){
  const auto mtls=getMTLSConfig();
  const auto ca=getCACertificates();
  NetworkOptions result;
  if(!mtls&&!ca)return result;
  const auto config=combine(mtls,ca);
  if(services_.bunRuntime){
    result.values["tls"]=config;
    return result;
  }
  debug("TLS: Created undici agent with custom certificates");
  if(!services_.makeNodeBat)
    throw std::invalid_argument("Missing Node TLS agent binding");
  Json connect=Json::object();
  for(const char* field:{"cert","key","passphrase","ca"})
    if(config.contains(field))connect[field]=config[field];
  result.dispatcher=services_.makeNodeBat(Json{{"connect",connect},
    {"pipelining",1}});
  if(!result.dispatcher)
    throw std::runtime_error("Node TLS agent returned no dispatcher");
  result.values["dispatcher"]=true;
  return result;
}
std::optional<Json> TlsOptions::getWebSocketTLSOptions(){
  const auto mtls=getMTLSConfig();
  const auto ca=getCACertificates();
  if(!mtls&&!ca)return std::nullopt;
  return combine(mtls,ca);
}
}
