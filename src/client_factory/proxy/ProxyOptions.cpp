#include "client_factory/proxy/ProxyOptions.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::client_factory::proxy {
namespace {
void merge(Json& target,const Json& source){
  if(!source.is_object())
    throw std::invalid_argument("TLS fetch options must be an object");
  for(auto it=source.begin();it!=source.end();++it)
    target[it.key()]=it.value();
}
}
ProxyOptions::ProxyOptions(Services services):
  services_(std::move(services)){}
void ProxyOptions::disableKeepAlive(){keepAliveDisabled_.store(true);}
void ProxyOptions::resetKeepAliveForTesting(){
  keepAliveDisabled_.store(false);
}
std::optional<std::string> ProxyOptions::env(const std::string& key)const{
  if(!services_.environment)
    throw std::invalid_argument("Missing proxy environment binding");
  return services_.environment(key);
}
std::optional<std::string> ProxyOptions::getProxyUrl()const{
  for(const char* key:{"https_proxy","HTTPS_PROXY",
      "http_proxy","HTTP_PROXY"}){
    const auto value=env(key);
    if(value&&!value->empty())return value;
  }
  return std::nullopt;
}
NetworkOptions ProxyOptions::getProxyFetchOptions(bool forApi)const{
  NetworkOptions result;
  if(keepAliveDisabled_.load())result.values["keepalive"]=false;
  if(forApi){
    const auto socket=env(services_.unixSocketKey);
    if(socket&&!socket->empty()&&services_.bunRuntime){
      result.values["unix"]=*socket;
      return result;
    }
  }
  const auto proxy=getProxyUrl();
  if(proxy){
    if(services_.bunRuntime){
      result.values["proxy"]=*proxy;
      if(!services_.getTLSFetchOptions)
        throw std::invalid_argument("Missing TLS fetch-options binding");
      auto tls=services_.getTLSFetchOptions();
      merge(result.values,tls.values);
      result.dispatcher=std::move(tls.dispatcher);
      return result;
    }
    if(!services_.getProxyBat)
      throw std::invalid_argument("Missing proxy agent binding");
    result.dispatcher=services_.getProxyBat(*proxy);
    if(!result.dispatcher)
      throw std::runtime_error("Proxy agent returned no dispatcher");
    result.values["dispatcher"]=true;
    return result;
  }
  if(!services_.getTLSFetchOptions)
    throw std::invalid_argument("Missing TLS fetch-options binding");
  auto tls=services_.getTLSFetchOptions();
  merge(result.values,tls.values);
  result.dispatcher=std::move(tls.dispatcher);
  return result;
}
}
