#include "client_factory/tls/TlsOptions.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::client_factory::tls {
TlsOptions::TlsOptions(Services services):services_(std::move(services)){}
std::optional<std::string> TlsOptions::env(const std::string& key)const{
  if(!services_.environment)
    throw std::invalid_argument("Missing TLS environment binding");
  return services_.environment(key);
}
void TlsOptions::debug(const std::string& message,
    const std::string& level)const{
  if(services_.log)services_.log(message,level);
}
std::shared_ptr<const Json> TlsOptions::getMTLSConfig(){
  if(mtlsStash_)return *mtlsStash_;
  Json config=Json::object();
  const auto certificatePath=env(services_.certKey);
  if(certificatePath&&!certificatePath->empty()){
    try{
      if(!services_.readFileUtf8)
        throw std::invalid_argument("Missing TLS file reader binding");
      const auto current=env(services_.certKey);
      if(!current)throw std::invalid_argument(
        "Missing client certificate path");
      config["cert"]=services_.readFileUtf8(*current);
      debug("mTLS: Loaded client certificate from BATBOX_CLIENT_CERT");
    }catch(const std::exception& error){
      debug("mTLS: Failed to load client certificate: "+
        std::string(error.what()),"error");
    }
  }
  const auto keyPath=env(services_.keyKey);
  if(keyPath&&!keyPath->empty()){
    try{
      if(!services_.readFileUtf8)
        throw std::invalid_argument("Missing TLS file reader binding");
      const auto current=env(services_.keyKey);
      if(!current)throw std::invalid_argument("Missing client key path");
      config["key"]=services_.readFileUtf8(*current);
      debug("mTLS: Loaded client key from BATBOX_CLIENT_KEY");
    }catch(const std::exception& error){
      debug("mTLS: Failed to load client key: "+
        std::string(error.what()),"error");
    }
  }
  const auto passphrase=env(services_.passphraseKey);
  if(passphrase&&!passphrase->empty()){
    config["passphrase"]=env(services_.passphraseKey).value_or("");
    debug("mTLS: Using client key passphrase");
  }
  if(config.empty())mtlsStash_=std::shared_ptr<const Json>{};
  else mtlsStash_=std::make_shared<const Json>(std::move(config));
  return *mtlsStash_;
}
void TlsOptions::clearMTLSStash(){
  mtlsStash_.reset();
  if(services_.clearMTLSBatStash)
    services_.clearMTLSBatStash();
  debug("Cleared mTLS configuration cache");
}
}
