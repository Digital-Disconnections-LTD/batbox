#include "cli/remote/WebSocketTransport.hpp"
#include "runtime/Url.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  std::string line;
  while(std::getline(std::cin,line)){
    const Json test=value::parse(line);
    Json events=Json::array();
    std::vector<cli::remote::WebSocketCallbacks> sockets;
    cli::remote::WebSocketServices services;
    services.nowMs=[]{return 1000.0;};
    services.randomUnit=[]{return 0.5;};
    services.logDebug=[](const auto&,const auto&){};
    services.registerFlightActivity=[&](auto){
      events.push_back(Json::array({"registerActivity"}));
    };
    services.unregisterFlightActivity=[&]{
      events.push_back(Json::array({"unregisterActivity"}));
    };
    services.jsonStringify=[](const Json& value){return value::stringify(value);};
    services.open=[&](const std::string& url,const Json& headers,
                      cli::remote::WebSocketCallbacks callbacks){
      events.push_back(Json::array({"socket",url,headers}));
      sockets.push_back(std::move(callbacks));
      auto socket=std::make_shared<cli::remote::WebSocketConnection>();
      socket->send=[&](const std::string& text){
        events.push_back(Json::array({"send",text}));
      };
      socket->ping=[&]{events.push_back(Json::array({"ping"}));};
      socket->close=[&]{events.push_back(Json::array({"socketClose"}));};
      socket->removeListeners=[]{};
      return socket;
    };
    const auto url=runtime::parseUrl("ws://localhost/session");
    cli::remote::WebSocketOptions options;
    options.autoReconnect=test.value("autoReconnect",true);
    auto transport=std::make_shared<cli::remote::WebSocketTransport>(loop,*url,
      Json{{"Authorization","Bearer old"}},"session-1",
      [&]{return Json{{"Authorization",test.value("freshToken",false)?
        "Bearer new":"Bearer old"}};},services,options);
    transport->setOnData([&](const std::string& value){
      events.push_back(Json::array({"data",value}));
    });
    transport->setOnClose([&](int code){
      events.push_back(Json::array({"close",code==0?Json():Json(code)}));
    });
    transport->setOnConnect([&]{
      events.push_back(Json::array({"connected"}));
    });
    for(const auto& action:test.at("actions")){
      const auto type=action.at("type").get<std::string>();
      if(type=="connect")transport->connect();
      else if(type=="write")transport->write(action.at("message"));
      else if(type=="open")sockets.back().onOpen(std::string());
      else if(type=="message")sockets.back().onMessage(
        action.at("data").get<std::string>());
      else if(type=="closeEvent")sockets.back().onClose(
        action.at("code").get<int>());
      else if(type=="pong")sockets.back().onPong();
      else if(type=="close")transport->close();
    }
    std::cout<<value::stringify(Json{{"events",events},
      {"state",transport->getStateLabel()}})<<'\n';
    transport.reset();
  }
}
