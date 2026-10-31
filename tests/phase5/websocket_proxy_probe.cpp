#include "cli/remote/WebSocketProxy.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
using namespace batbox::cli::remote;
int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=value::parse(line);
    const auto url=input.at("url").get<std::string>();
    const auto& env=input.at("env");
    ProxyEnvironment environment=[&](const std::string& key)->
        std::optional<std::string>{
      if(!env.contains(key)||!env[key].is_string())return {};
      return env[key].get<std::string>();
    };
    const auto proxy=getWebSocketProxyUrl(url,environment);
    auto noProxy=environment("no_proxy");
    if(!noProxy||noProxy->empty())noProxy=environment("NO_PROXY");
    std::cout<<value::stringify(Json{{"proxy",proxy?Json(*proxy):Json(nullptr)},
      {"bypass",shouldBypassWebSocketProxy(url,noProxy)}})<<'\n';
  }catch(const std::exception& error){
    std::cerr<<error.what()<<'\n';return 1;
  }
}
