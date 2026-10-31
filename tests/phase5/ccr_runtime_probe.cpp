#include "cli/remote/CcrRuntime.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/Url.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace batbox;
using namespace batbox::cli::remote;
namespace {
template<class T>T await(event_loop::Loop& loop,
    event_loop::Deferred<T> pending){
  std::optional<T> value;
  std::exception_ptr error;
  pending.then([&](const T* result,const std::exception_ptr& failure){
    if(failure)error=failure;
    else if(result)value=*result;
  });
  for(int turn=0;turn<5000&&!value&&!error;++turn){
    loop.runReady();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if(error)std::rethrow_exception(error);
  if(!value)throw std::runtime_error("CCR operation pending");
  return *value;
}
Json run(const Json& test){
  event_loop::Loop loop;
  Json requests=Json::array();
  std::vector<int> eventStatuses,getStatuses;
  if(test.contains("eventStatuses"))for(const auto& value:test["eventStatuses"])
    eventStatuses.push_back(value.get<int>());
  if(test.contains("getStatuses"))for(const auto& value:test["getStatuses"])
    getStatuses.push_back(value.get<int>());
  SseServices sse;
  sse.openStream=[](const auto&,const auto&,auto){
    auto handle=std::make_shared<SseStreamHandle>();
    handle->abort=[]{};return handle;
  };
  sse.post=[&](const auto&,const auto&,const auto&){
    event_loop::Deferred<SseHttpResponse> result(loop);
    result.resolve({200,nullptr});return result;
  };
  sse.getAuthHeaders=[]{return Json{{"Authorization","Bearer local"}};};
  sse.userBat=[]{return "batbox-test";};
  sse.nowMs=[]{return 1000.0;};sse.randomUnit=[]{return 0.5;};
  sse.jsonStringify=[](const Json& value){return value::stringify(value);};
  sse.logDebug=[](const auto&,const auto&){};
  sse.protocolVersionHeaderName="anthropic-version";
  sse.protocolVersionHeaderValue="2023-06-01";
  const auto streamUrl=runtime::parseUrl(
    "https://localhost/v1/code/sessions/session-1/worker/events/stream");
  const auto flightUrl=runtime::parseUrl(
    "https://localhost/v1/code/sessions/session-1");
  if(!streamUrl||!flightUrl)throw std::runtime_error("CCR URL parse failed");
  auto transport=std::make_shared<SSETransport>(loop,*streamUrl,
    Json::object(),"session-1",std::function<Json()>{},std::nullopt,sse);
  CcrRuntimeServices services;
  services.http=[&](const std::string& method,const std::string& url,
      const Json& body,const Json& headers,double timeout){
    requests.push_back(Json{{"method",method},{"url",url},{"body",body},
      {"headers",headers},{"timeout",timeout}});
    event_loop::Deferred<CcrHttpResponse> result(loop);
    CcrHttpResponse response;
    response.status=200;
    if(method=="get"){
      if(!getStatuses.empty()){
        response.status=getStatuses.front();getStatuses.erase(getStatuses.begin());
      }
      if(url.ends_with("/worker"))
        response.data=Json{{"worker",{{"external_metadata",
          {{"restored",true}}}}}};
      else response.data=Json{{"data",Json::array(
        {Json{{"event_id","internal-1"}}})}};
    }
    if(method=="put"&&test.contains("initStatus")&&
       value::property(body,"worker_status")=="idle")
      response.status=test["initStatus"].get<int>();
    if(method=="post"&&test.contains("eventStatus")&&
       url.ends_with("/worker/events"))
      response.status=test["eventStatus"].get<int>();
    if(method=="post"&&url.ends_with("/worker/events")&&
       !eventStatuses.empty()){
      response.status=eventStatuses.front();
      eventStatuses.erase(eventStatuses.begin());
    }
    if(response.status==429)response.headers["retry-after"]="1";
    result.resolve(std::move(response));return result;
  };
  services.getAuthHeaders=[&]{return test.value("auth",true)?
    Json{{"Authorization","Bearer local"}}:Json::object();};
  services.getFlightIngressAuthToken=[]()->std::optional<std::string>{
    return "local-token";
  };
  services.decodeJwtExpiry=[](const auto&)->std::optional<double>{
    return {};
  };
  services.getEnvironmentWorkerEpoch=[&]()->std::optional<std::string>{
    return test.value("envEpoch",std::string("3"));
  };
  services.userBat=[]{return "batbox-test";};
  int uuid=0;services.randomUUID=[&]{return "uuid-"+
    std::to_string(++uuid);};
  services.nowMs=[]{return 1000.0;};
  services.randomUnit=[]{return 0.5;};
  services.registerFlightActivity=[](auto){};
  services.unregisterFlightActivity=[]{};
  services.onEpochMismatch=[](){throw std::runtime_error("epoch mismatch");};
  services.logDebug=[](const auto&,const auto&){};
  services.protocolVersionHeaderName="anthropic-version";
  services.protocolVersionHeaderValue="2023-06-01";
  auto client=std::make_shared<CcrRuntime>(loop,transport,*flightUrl,services);
  client->attachTransportEvents();
  Json restored=nullptr,read=nullptr,error=nullptr;
  for(const auto& action:test.at("actions")){
    const auto op=action.at("op").get<std::string>();
    try{
      if(op=="initialize"){
        std::optional<double> epoch;
        if(action.contains("epoch"))epoch=action["epoch"].get<double>();
        restored=await(loop,client->initialize(epoch));
      }else if(op=="write")await(loop,client->writeEvent(
        action.at("message")));
      else if(op=="internal")await(loop,client->writeInternalEvent(
        action.at("type").get<std::string>(),action.at("payload"),
        action.value("options",Json::object()).value(
          "isCompaction",false),
        action.value("options",Json::object()).contains("agentId")?
          std::optional<std::string>(action["options"]["agentId"]
            .get<std::string>()):std::nullopt));
      else if(op=="flush")await(loop,client->flush());
      else if(op=="flushInternal")await(loop,client->flushInternalEvents());
      else if(op=="state")client->reportState(action.at("state"),
        action.contains("details")?std::optional<Json>(
          action["details"]):std::nullopt);
      else if(op=="metadata")client->reportMetadata(action.at("metadata"));
      else if(op=="delivery"||op=="event")
        client->reportDelivery(op=="event"?
          action.at("event").at("event_id").get<std::string>():
          action.at("id").get<std::string>(),
          op=="event"?"received":action.at("status").get<std::string>());
      else if(op=="read")read=await(loop,client->readInternalEvents());
      else if(op=="readSubagent")read=await(loop,
        client->readSubagentInternalEvents());
      else if(op=="close")client->close();
    }catch(const CcrInitError& cause){
      error=Json{{"name","Error"},{"reason",cause.reason()}};break;
    }catch(const std::exception&){
      error=Json{{"name","Error"},{"reason",nullptr}};break;
    }
  }
  for(int turn=0;turn<40;++turn)loop.runReady();
  return Json{{"requests",requests},{"restored",restored},
    {"error",error},{"read",read},{"epoch",client->getWorkerEpoch()},
    {"pending",client->internalEventsPending()}};
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
