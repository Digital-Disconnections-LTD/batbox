#include "cli/remote/TransportSelection.hpp"
#include "runtime/Url.hpp"
#include <iostream>
#include <string>

using namespace batbox;
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const Json test=value::parse(line);
    Json events=Json::array();Json error;
    cli::remote::TransportSelectionServices services;
    services.ccrV2Enabled=[&]{return test.value("ccr",false);};
    services.postForIngressEnabled=[&]{return test.value("post",false);};
    auto factory=[&](const std::string& kind){
      return [&,kind](const Json& url,const Json& headers,
          const std::string& id,std::function<Json()> refresh){
        events.push_back(Json::array({kind,value::property(url,"href"),
          headers,id,refresh()}));
        return std::make_shared<cli::remote::Transport>();
      };
    };
    services.makeSseTransport=factory("sse");
    services.makeHybridTransport=factory("hybrid");
    services.makeWebSocketTransport=factory("ws");
    try{
      const auto parsed=runtime::parseUrl(test.at("url").get<std::string>());
      if(!parsed)throw std::invalid_argument("Invalid URL");
      cli::remote::getTransportForUrl(*parsed,
        Json{{"Authorization","Bearer a"}},"session-1",
        []{return Json{{"Authorization","Bearer b"}};},services);
    }catch(const std::exception& failure){error=failure.what();}
    std::cout<<value::stringify(Json{{"events",events},{"error",error}})<<'\n';
  }
}
