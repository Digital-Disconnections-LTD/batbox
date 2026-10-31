#include "client_factory/tls/IndigenousClientBinding.hpp"
#include "client_factory/tls/IndigenousCerts.hpp"
#include <memory>
#include <mutex>

namespace batbox::client_factory::tls {
std::function<void(transport::ClientConfig&)> makeIndigenousTlsClientConfigurator(
    std::function<void(const std::string&,const std::string&)> log){
  struct State {
    std::mutex mutex;
    TlsOptions options;
    explicit State(Services services):options(std::move(services)){}
  };
  const auto state=std::make_shared<State>(makeIndigenousTlsServices({},std::move(log)));
  return [state](transport::ClientConfig& config){
    std::lock_guard lock(state->mutex);
    const auto options=state->options.getTLSFetchOptions();
    const auto& tls=value::property(options.values,"tls");
    if(!tls.is_object())return;
    if(tls.contains("ca"))config.tlsCaPem=tls.at("ca").get<std::vector<std::string>>();
    if(tls.contains("cert"))config.tlsCertificatePem=tls.at("cert").get<std::string>();
    if(tls.contains("key"))config.tlsPrivateKeyPem=tls.at("key").get<std::string>();
    if(tls.contains("passphrase"))config.tlsKeyPassphrase=tls.at("passphrase").get<std::string>();
  };
}
}
