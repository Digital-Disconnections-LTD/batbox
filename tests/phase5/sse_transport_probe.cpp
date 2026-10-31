#include "cli/remote/SSETransport.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/Url.hpp"
#include <iostream>
#include <sstream>

using namespace batbox;
using namespace batbox::cli::remote;
namespace {
Json array(){return Json::array();}
std::string text(const Json& value,const char* key,
  const std::string& fallback=""){
  const auto& field=value::property(value,key);
  return field.is_string()?field.get<std::string>():fallback;
}
Json run(const Json& input){
  event_loop::Loop loop;
  Json events=array(),posts=array(),gets=array();
  SseStreamCallbacks callbacks;
  SseServices services;
  services.openStream=[&](const std::string& url,const Json& headers,
      SseStreamCallbacks next){
    gets.push_back(Json{{"url",url},{"headers",headers}});
    callbacks=std::move(next);
    auto handle=std::make_shared<SseStreamHandle>();
    handle->abort=[]{};
    return handle;
  };
  services.post=[&](const std::string& url,const Json& headers,
      const Json& message){
    posts.push_back(Json{{"url",url},{"headers",headers},
      {"message",message}});
    event_loop::Deferred<SseHttpResponse> result(loop);
    result.resolve(SseHttpResponse{
      value::property(input,"postStatus").is_number_integer()?
        value::property(input,"postStatus").get<int>():200,Json::object()});
    return result;
  };
  services.getAuthHeaders=[&]{
    return input.contains("auth")?input["auth"]:
      Json{{"Authorization","Bearer token"}};
  };
  services.userBat=[]{return "batbox-test";};
  services.nowMs=[]{return 1000.0;};
  services.randomUnit=[]{return 0.5;};
  services.jsonStringify=[](const Json& value){return value::stringify(value);};
  services.logDebug=[](const std::string&,const Json&){};
  services.protocolVersionHeaderName="anthropic-version";
  services.protocolVersionHeaderValue="2023-06-01";
  const auto url=runtime::parseUrl(text(input,"url",
    "https://localhost/session/events/stream?x=1"));
  if(!url)throw std::runtime_error("URL parse failure");
  std::optional<double> initial;
  if(value::property(input,"initialSeq").is_number())
    initial=value::property(input,"initialSeq").get<double>();
  auto transport=std::make_shared<SSETransport>(loop,*url,
    input.contains("headers")?input["headers"]:Json::object(),
    "session-1",std::function<Json()>{},initial,std::move(services));
  transport->setOnData([&](const std::string& value){
    events.push_back(Json::array({"data",value}));
  });
  transport->setOnClose([&](std::optional<int> value){
    events.push_back(Json::array({"close",
      value?Json(*value):Json(nullptr)}));
  });
  transport->setOnEvent([&](const StreamClientEvent& value){
    events.push_back(Json::array({"event",value.value}));
  });
  if(!input.contains("connect")||input["connect"]!=false){
    transport->connect();
    const int status=value::property(input,"status").is_number_integer()?
      input["status"].get<int>():200;
    callbacks.onHeaders(status,
      !input.contains("noBody")||input["noBody"]!=true);
    if(status>=200&&status<300&&
       (!input.contains("noBody")||input["noBody"]!=true)){
      if(input.contains("chunks"))for(const auto& chunk:input["chunks"])
        callbacks.onBytes(chunk.get<std::string>());
      callbacks.onEnd(std::nullopt);
    }
  }
  if(input.contains("message"))transport->write(input["message"]);
  loop.runReady();
  return Json{{"events",events},{"lastSeq",transport->getLastSequenceNum()},
    {"state",transport->getStateLabel()},{"posts",posts},{"gets",gets}};
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
