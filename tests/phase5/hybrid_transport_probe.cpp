#include "cli/remote/HybridTransport.hpp"
#include "runtime/Url.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace batbox;
using namespace batbox::cli::remote;
namespace {
void await(event_loop::Loop& loop,event_loop::Deferred<event_loop::Unit> mission){
  for(int i=0;i<1000&&!mission.settled();++i){
    loop.runReady();std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if(!mission.settled())throw std::runtime_error("Hybrid operation pending");
}
Json run(const Json& test){
  event_loop::Loop loop;
  Json posts=Json::array();
  HybridServices services;
  services.websocket.open=[](const auto&,const auto&,auto){
    auto socket=std::make_shared<WebSocketConnection>();
    socket->send=[](const auto&){};socket->close=[]{};
    socket->removeListeners=[]{};return socket;
  };
  services.websocket.nowMs=[]{return 1000.0;};
  services.websocket.randomUnit=[]{return 0.5;};
  services.websocket.logDebug=[](const auto&,const auto&){};
  services.websocket.registerFlightActivity=[](auto){};
  services.websocket.unregisterFlightActivity=[]{};
  services.websocket.jsonStringify=[](const Json& value){
    return value::stringify(value);
  };
  services.getFlightIngressAuthToken=[&]()->std::optional<std::string>{
    if(test.contains("token")&&test["token"]==false)return {};
    return "local-token";
  };
  services.post=[&](const std::string& url,const Json& headers,
      const Json& body){
    posts.push_back(Json{{"url",url},{"body",body},{"headers",headers}});
    event_loop::Deferred<SseHttpResponse> result(loop);
    result.resolve(SseHttpResponse{test.value("status",200),nullptr});
    return result;
  };
  services.logDebug=[](const auto&,const auto&){};
  services.randomUnit=[]{return 0.0;};
  services.jsonStringify=[](const Json& value){return value::stringify(value);};
  if(test.contains("maxFailures"))
    services.maxConsecutiveFailures=
      test["maxFailures"].get<std::size_t>();
  const auto url=runtime::parseUrl(test.value("url",
    std::string("ws://localhost/v2/ws/session-1?x=1")));
  if(!url)throw std::runtime_error("Hybrid URL parse failure");
  auto transport=std::make_shared<HybridTransport>(loop,*url,Json::object(),
    "session-1",std::function<Json()>{},std::move(services));
  for(const auto& action:test.at("actions")){
    const auto type=action.at("type").get<std::string>();
    if(type=="write")await(loop,transport->write(action.at("message")));
    else if(type=="batch")
      await(loop,transport->writeBatch(
        action.at("messages").get<std::vector<Json>>()));
    else if(type=="flush")await(loop,transport->flush());
  }
  await(loop,transport->flush());
  return Json{{"posts",posts},{"dropCount",transport->droppedBatchCount()}};
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    std::cout<<value::stringify(run(value::parse(line)))<<'\n';
  }catch(const std::exception& error){
    std::cerr<<error.what()<<'\n';return 1;
  }
}
