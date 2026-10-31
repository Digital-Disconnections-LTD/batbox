#include "cli/structured/RequestProducer.hpp"
#include "cli/structured/ProcessLine.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <optional>
#include <stop_token>
#include <string>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  std::string line;
  while(std::getline(std::cin,line)){
    const Json input=value::parse(line);
    Json events=Json::array();
    auto outbound=std::make_shared<cli::structured::AsyncStream<Json>>(loop);
    auto control=std::make_shared<cli::structured::ControlResponses>(
      [](const Json&){});
    cli::structured::RequestProducerServices services;
    services.randomUUID=[]{return std::string("generated");};
    if(input.value("onSent",false))services.onControlRequestSent=[&](
        const Json& message){
      events.push_back(Json::array({"sent",message}));
    };
    cli::structured::RequestProducer producer(loop,control,outbound,services);
    if(input.value("closed",false))producer.closeInput();
    std::stop_source controller;
    if(input.value("abortBefore",false))controller.request_stop();
    const auto signal=input.value("signal",false)?controller.get_token():
      std::stop_token{};
    const auto& idValue=value::property(input,"requestId");
    const std::optional<std::string> id=idValue.is_string()?
      std::optional<std::string>(idValue.get<std::string>()):std::nullopt;
    const auto requestId=id.value_or("generated");
    const Json request=input.at("request");
    auto promise=producer.sendRequest(request,
      [](const Json& value){return value;},signal,id);
    const Json before={{"events",events},
      {"outbound",Json(outbound->snapshotQueued())},
      {"pending",control->hasPendingRequest(requestId)}};
    std::optional<Json> afterAbort;
    if(input.value("abortAfter",false)){
      controller.request_stop();
      afterAbort=Json{{"outbound",Json(outbound->snapshotQueued())},
        {"pending",control->hasPendingRequest(requestId)}};
    }
    else{
      const auto settle=input.value("settle",std::string());
      if(!settle.empty()){
        cli::structured::ProcessLineServices processing;
        processing.notifyCommandLifecycle=[](const auto&,const auto&){};
        const auto reply=value::stringify(Json{{"type","control_response"},
          {"response",Json{{"request_id",requestId},
            {"subtype",settle=="reject"?"error":"success"},
            {"response",value::property(input,"response")},
            {"error","denied"}}}});
        cli::structured::processLine(loop,reply,*control,processing);
      }
    }
    std::optional<Json> value;
    std::exception_ptr error;
    promise.then([&](const Json* result,const std::exception_ptr& failure){
      error=failure;if(result)value=*result;
    });
    loop.runReady();
    Json errorText,name;
    if(error){
      try{std::rethrow_exception(error);}
      catch(const cli::structured::RequestAbortError& cause){
        errorText=cause.what();name="AbortError";
      }catch(const std::exception& cause){
        errorText=cause.what();name="Error";
      }
    }
    const auto& gadgetUse=value::property(request,"tool_use_id");
    const bool resolved=gadgetUse.is_string()&&
      control->hasResolvedGadgetUseId(gadgetUse.get<std::string>());
    const Json after={{"events",events},
      {"outbound",Json(outbound->snapshotQueued())},
      {"pending",control->hasPendingRequest(requestId)},
      {"resolved",resolved}};
    Json result={{"before",before},{"after",after},
      {"value",value?*value:Json()},{"error",errorText},{"name",name}};
    if(afterAbort)result["afterAbort"]=*afterAbort;
    std::cout<<value::stringify(result)<<'\n';
  }
}
