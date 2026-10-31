#include "client_factory/ClientFactory.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace batbox::client_factory {
namespace {
bool indigenousNetworkOptions(const Json& options,
    transport::ClientConfig& config){
  if(!options.is_object())return false;
  for(auto it=options.begin();it!=options.end();++it){
    const auto& key=it.key();
    const auto& value=it.value();
    if(key=="keepalive"){
      if(value!=false)return false;
      config.disableKeepAlive=true;
    }else if(key=="proxy"){
      if(!value.is_string())return false;
      config.proxyUrl=value.get<std::string>();
    }else if(key=="unix"){
      if(!value.is_string())return false;
      config.unixSocketPath=value.get<std::string>();
    }else if(key=="tls"){
      if(!value.is_object())return false;
      for(auto tls=value.begin();tls!=value.end();++tls){
        const auto& name=tls.key();
        const auto& part=tls.value();
        if(name=="ca"){
          if(part.is_string())config.tlsCaPem={part.get<std::string>()};
          else if(part.is_array()){
            for(const auto& certificate:part){
              if(!certificate.is_string())return false;
              config.tlsCaPem.push_back(certificate.get<std::string>());
            }
          }else return false;
        }else if(part.is_string()){
          if(name=="cert")config.tlsCertificatePem=part.get<std::string>();
          else if(name=="key")config.tlsPrivateKeyPem=part.get<std::string>();
          else if(name=="passphrase")
            config.tlsKeyPassphrase=part.get<std::string>();
          else return false;
        }else return false;
      }
    }else return false;
  }
  return config.proxyUrl.empty()||config.unixSocketPath.empty();
}
}
std::string providerName(Provider provider){
  switch(provider){
    case Provider::Direct:return "direct";
    case Provider::Bedrock:return "bedrock";
    case Provider::Foundry:return "foundry";
    case Provider::Vertex:return "vertex";
  }
  throw std::invalid_argument("Unknown client provider");
}
SdkLogger createStderrLogger(const Services& services){
  if(!services.writeStderr)
    throw std::invalid_argument("Missing SDK stderr writer binding");
  auto make=[write=services.writeStderr](const std::string& level){
    return [write,level](const std::string& message,const Json& args){
      write("[Batbox SDK "+level+"]",message,args);
    };
  };
  return {make("ERROR"),make("WARN"),make("INFO"),make("DEBUG")};
}
std::shared_ptr<transport::Client> makeIndigenousDirectClient(
    const ClientPlan& plan,const Services& services){
  if(plan.provider!=Provider::Direct)
    throw std::invalid_argument("Native direct client requires direct plan");
  transport::ClientConfig config;
  const bool indigenousOptions=indigenousNetworkOptions(plan.proxyFetchOptions,
    config);
  if(plan.fetchOverride||plan.proxyDispatcher||!indigenousOptions||
      plan.maxRetries!=0||
      plan.stderrLogger){
    if(!services.makeDirectNetworkClient)
      throw std::invalid_argument(
        "Direct network options require a bound native network adapter");
    auto client=services.makeDirectNetworkClient(plan);
    if(!client)throw std::runtime_error("Direct network adapter returned no client");
    return client;
  }
  if(!std::isfinite(plan.timeoutMs)||plan.timeoutMs<0||
      plan.timeoutMs>static_cast<double>(std::numeric_limits<long long>::max()))
    throw std::invalid_argument("Invalid API timeout");
  config.baseUrl=plan.baseUrl.value_or(services.directBaseUrl);
  config.messagesPath=services.messagesPath;
  config.headers=plan.defaultHeaders;
  config.protocol=services.messagesProtocol;
  config.requestTimeout=std::chrono::milliseconds(
    static_cast<long long>(plan.timeoutMs));
  if(plan.apiKey&& !plan.apiKey->empty()){
    if(services.wire.apiKey.empty())
      throw std::invalid_argument("Missing API-key wire header binding");
    config.headers[services.wire.apiKey]=*plan.apiKey;
  }
  if(plan.authToken)config.authToken=*plan.authToken;
  return std::make_shared<transport::Client>(std::move(config));
}
}
