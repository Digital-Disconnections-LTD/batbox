#include "client_factory/tls/TlsOptions.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array();
    client_factory::tls::Services services;
    services.bunRuntime=test.value("bun",false);
    services.systemCaApiAvailable=test.value("systemApi",true);
    services.environment=[&](const std::string& key)->
        std::optional<std::string>{
      trace.push_back(Json::array({"env",key}));
      const auto& s=services;
      const char* name=nullptr;
      if(key==s.optionsKey)name="options";
      else if(key==s.extraCaKey)name="extraPath";
      else if(key==s.certKey)name="certPath";
      else if(key==s.keyKey)name="keyPath";
      else if(key==s.passphraseKey)name="passphrase";
      if(name&&test.contains(name)&&!test[name].is_null())
        return test[name].get<std::string>();
      return std::nullopt;
    };
    services.readFileUtf8=[&](const std::string& path){
      trace.push_back(Json::array({"read",path}));
      const auto files=test.value("files",Json::object());
      if(!files.contains(path))throw std::runtime_error("Error: missing file");
      return files[path].get<std::string>();
    };
    services.prepareCertificateProvider=[&]{
      trace.push_back(Json::array({"require","tls"}));};
    services.bundledRootCertificates=[&]{
      return test.value("roots",Json::array({"BASE"})).
        get<std::vector<std::string>>();
    };
    services.systemCaCertificates=[&]()->
        std::optional<std::vector<std::string>>{
      trace.push_back(Json::array({"system","system"}));
      return test.value("systemRoots",Json::array()).
        get<std::vector<std::string>>();
    };
    services.makeNodeBat=[&](const Json& options){
      trace.push_back(Json::array({"require","undici"}));
      trace.push_back(Json::array({"agent",options}));
      return std::shared_ptr<void>(std::make_shared<int>(1));
    };
    services.clearMTLSBatStash=[](){};
    services.log=[&](const std::string& message,
        const std::string& level){
      trace.push_back(Json::array({"log",message,level}));
    };
    client_factory::tls::TlsOptions value(services);
    const auto mtls=value.getMTLSConfig();
    const auto ca=value.getCACertificates();
    const auto fetch=value.getTLSFetchOptions();
    const auto websocket=value.getWebSocketTLSOptions();
    const Json first={{"mtls",mtls?*mtls:Json(nullptr)},
      {"ca",ca?Json(*ca):Json(nullptr)},
      {"fetch",fetch.values},
      {"websocket",websocket.value_or(Json(nullptr))},
      {"mtlsSame",mtls==value.getMTLSConfig()},
      {"caSame",ca==value.getCACertificates()},
      {"option",value.hasRuntimeOption(test.value("checkFlag",
        std::string("--use-system-ca")))}};
    Json after=nullptr;
    if(test.value("clear",false)){
      value.clearCACertsStash();value.clearMTLSStash();
      const auto freshM=value.getMTLSConfig();
      const auto freshC=value.getCACertificates();
      after=Json{{"mtls",freshM?*freshM:Json(nullptr)},
        {"ca",freshC?Json(*freshC):Json(nullptr)},
        {"fetch",value.getTLSFetchOptions().values}};
    }
    std::cout<<value::stringify(Json{{"value",first},
      {"after",after},{"trace",trace}})<<'\n';
  }
}
