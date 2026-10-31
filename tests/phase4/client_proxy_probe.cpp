#include "client_factory/proxy/ProxyOptions.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array();
    client_factory::proxy::Services services;
    services.bunRuntime=test.value("bun",false);
    services.environment=[&](const std::string& key)->
        std::optional<std::string>{
      trace.push_back(Json::array({"env",key}));
      if(key==services.unixSocketKey){
        if(test.contains("unix")&&!test["unix"].is_null())
          return test["unix"].get<std::string>();
        return std::nullopt;
      }
      const auto env=test.value("env",Json::object());
      if(!env.contains(key)||env[key].is_null())return std::nullopt;
      return env[key].get<std::string>();
    };
    services.getTLSFetchOptions=[&]{
      trace.push_back("tls");
      client_factory::NetworkOptions result;
      result.values=test.value("tlsOptions",Json::object());
      if(result.values.contains("dispatcher")&&
          value::truthy(result.values["dispatcher"])){
        result.values["dispatcher"]=true;
        result.dispatcher=std::make_shared<int>(1);
      }
      return result;
    };
    services.getProxyBat=[&](const std::string& uri){
      trace.push_back(Json::array({"agent",uri}));
      return std::shared_ptr<void>(std::make_shared<int>(1));
    };
    client_factory::proxy::ProxyOptions proxy(services);
    if(test.value("disable",false))proxy.disableKeepAlive();
    Json value;
    const auto forApi=test.value("forApi",false);
    if(test.value("op",std::string())=="url"){
      const auto url=proxy.getProxyUrl();
      value=url?Json(*url):Json(nullptr);
    }else value=proxy.getProxyFetchOptions(forApi).values;
    Json after=nullptr;
    if(test.value("reset",false)){
      proxy.resetKeepAliveForTesting();
      after=proxy.getProxyFetchOptions(forApi).values;
    }
    std::cout<<value::stringify(Json{{"value",value},
      {"after",after},{"trace",trace}})<<'\n';
  }
}
