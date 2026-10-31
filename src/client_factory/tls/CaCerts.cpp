#include "client_factory/tls/TlsOptions.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::client_factory::tls {
namespace {
bool jsWhitespace(char16_t value){
  return value==0x0009||value==0x000a||value==0x000b||
    value==0x000c||value==0x000d||value==0x0020||
    value==0x00a0||value==0x1680||
    (value>=0x2000&&value<=0x200a)||value==0x2028||
    value==0x2029||value==0x202f||value==0x205f||
    value==0x3000||value==0xfeff;
}
}
bool TlsOptions::hasRuntimeOption(const std::string& flag)const{
  const auto options=env(services_.optionsKey);
  if(!options||options->empty())return false;
  const auto units=value::utf16CodeUnits(*options);
  const auto wanted=value::utf16CodeUnits(flag);
  std::size_t begin=0;
  for(;;){
    auto end=begin;
    while(end<units.size()&&!jsWhitespace(units[end]))++end;
    if(units.substr(begin,end-begin)==wanted)return true;
    if(end==units.size())break;
    while(end<units.size()&&jsWhitespace(units[end]))++end;
    begin=end;
  }
  return false;
}
std::shared_ptr<const std::vector<std::string>>
TlsOptions::getCACertificates(){
  if(caStash_)return *caStash_;
  const bool useSystemCA=hasRuntimeOption("--use-system-ca")||
    hasRuntimeOption("--use-openssl-ca");
  const auto extraPath=env(services_.extraCaKey);
  debug("CA certs: useSystemCA="+std::string(useSystemCA?"true":"false")+
    ", extraCertsPath="+extraPath.value_or("undefined"));
  if(!useSystemCA&&(!extraPath||extraPath->empty())){
    caStash_=std::shared_ptr<const std::vector<std::string>>{};
    return *caStash_;
  }
  if(services_.prepareCertificateProvider)
    services_.prepareCertificateProvider();
  std::vector<std::string> certs;
  if(useSystemCA){
    std::optional<std::vector<std::string>> system;
    if(services_.systemCaApiAvailable){
      if(!services_.systemCaCertificates)
        throw std::invalid_argument("Missing system CA binding");
      system=services_.systemCaCertificates();
    }
    if(system&&!system->empty()){
      certs.insert(certs.end(),system->begin(),system->end());
      debug("CA certs: Loaded "+std::to_string(certs.size())+
        " system CA certificates (--use-system-ca)");
    }else if(!services_.systemCaApiAvailable&&
        (!extraPath||extraPath->empty())){
      debug("CA certs: --use-system-ca set but system CA API "
        "unavailable, deferring to runtime");
      caStash_=std::shared_ptr<const std::vector<std::string>>{};
      return *caStash_;
    }else{
      if(!services_.bundledRootCertificates)
        throw std::invalid_argument("Missing bundled CA binding");
      auto roots=services_.bundledRootCertificates();
      certs.insert(certs.end(),roots.begin(),roots.end());
      debug("CA certs: Loaded "+std::to_string(certs.size())+
        " bundled root certificates as base (--use-system-ca fallback)");
    }
  }else{
    if(!services_.bundledRootCertificates)
      throw std::invalid_argument("Missing bundled CA binding");
    auto roots=services_.bundledRootCertificates();
    certs.insert(certs.end(),roots.begin(),roots.end());
    debug("CA certs: Loaded "+std::to_string(certs.size())+
      " bundled root certificates as base");
  }
  if(extraPath&&!extraPath->empty()){
    try{
      if(!services_.readFileUtf8)
        throw std::invalid_argument("Missing TLS file reader binding");
      certs.push_back(services_.readFileUtf8(*extraPath));
      debug("CA certs: Appended extra certificates from "
        "BATBOX_EXTRA_CA_CERTS ("+*extraPath+")");
    }catch(const std::exception& error){
      debug("CA certs: Failed to read BATBOX_EXTRA_CA_CERTS file ("+
        *extraPath+"): "+error.what(),"error");
    }
  }
  if(certs.empty())caStash_=
    std::shared_ptr<const std::vector<std::string>>{};
  else caStash_=std::make_shared<const std::vector<std::string>>(
    std::move(certs));
  return *caStash_;
}
void TlsOptions::clearCACertsStash(){
  caStash_.reset();
  debug("Cleared CA certificates cache");
}
}
