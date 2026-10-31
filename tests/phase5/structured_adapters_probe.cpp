#include "cli/structured/ControlAdapters.hpp"
#include "cli/structured/ProcessLine.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <memory>
#include <optional>
#include <string>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  std::string line;
  while(std::getline(std::cin,line)){
    const Json input=value::parse(line);
    Json events=Json::array();
    int serial=0;
    auto uuid=[&]{return "id"+std::to_string(++serial);};
    auto control=std::make_shared<cli::structured::ControlResponses>(
      [](const Json&){});
    auto outbound=std::make_shared<cli::structured::AsyncStream<Json>>(loop);
    auto producer=std::make_shared<cli::structured::RequestProducer>(loop,
      control,outbound,cli::structured::RequestProducerServices{uuid,{}});
    if(input.value("error",false))producer->closeInput();
    cli::structured::ControlAdapterServices services;
    services.randomUUID=uuid;
    services.logHookError=[&](const std::string&,
                              const std::string& error){
      events.push_back(Json::array({"hook-error",error}));
    };
    const auto identity=[](const Json& value){return value;};
    services.schemas.hookOutput=identity;
    services.schemas.elicitationResponse=identity;
    services.schemas.permissionOutput=identity;
    services.schemas.mcpResponse=identity;
    const auto method=input.at("method").get<std::string>();
    std::optional<event_loop::Deferred<Json>> pendingJson;
    std::optional<event_loop::Deferred<bool>> pendingBool;
    Json descriptor;
    if(method=="hook"){
      const auto& timeout=value::property(input,"timeout");
      const auto timeoutValue=timeout.is_number()?
        std::optional<double>(timeout.get<double>()):std::nullopt;
      auto callback=cli::structured::createHookCallback(loop,producer,
        "callback-1",timeoutValue,services);
      descriptor={{"type",callback.type}};
      if(callback.timeout)descriptor["timeout"]=*callback.timeout;
      const auto& gadgetUseID=value::property(input,"toolUseId");
      const auto gadgetValue=gadgetUseID.is_string()?
        std::optional<std::string>(gadgetUseID.get<std::string>()):
        std::nullopt;
      pendingJson=callback.callback(input.at("payload"),gadgetValue,{});
    }else if(method=="elicitation"){
      const auto optionalJson=[&](const char* key)->std::optional<Json>{
        const auto& value=value::property(input,key);
        return value.is_null()?std::nullopt:std::optional<Json>(value);
      };
      const auto optionalText=[&](const char* key)->std::optional<std::string>{
        const auto& value=value::property(input,key);
        return value.is_string()?std::optional<std::string>(
          value.get<std::string>()):std::nullopt;
      };
      pendingJson=cli::structured::handleElicitation(loop,producer,
        "server-1","Please enter",optionalJson("requestedSchema"),{},
        optionalText("mode"),optionalText("url"),
        optionalText("elicitationId"),services);
    }else if(method=="sandbox"){
      auto ask=cli::structured::createSandboxAskCallback(loop,producer,
        services);
      pendingBool=ask(Json{{"host",input.at("host")},{"port",443}});
    }else pendingJson=cli::structured::sendMcpMessage(loop,producer,
      "server-1",Json{{"jsonrpc","2.0"},{"id",1},
        {"method","tools/list"}},services);
    const Json before={{"events",events},
      {"outbound",Json(outbound->snapshotQueued())}};
    if(!input.value("error",false)){
      const auto queued=outbound->snapshotQueued();
      if(queued.empty())throw std::runtime_error("Missing outbound request");
      const auto id=queued.front().at("request_id").get<std::string>();
      const Json reply={{"type","control_response"},
        {"response",Json{{"request_id",id},{"subtype","success"},
          {"response",value::property(input,"response")}}}};
      cli::structured::ProcessLineServices processing;
      processing.notifyCommandLifecycle=[](const auto&,const auto&){};
      cli::structured::processLine(loop,value::stringify(reply),*control,
                                   processing);
    }
    Json value,error;
    if(pendingJson)pendingJson->then([&](const Json* result,
                                         const std::exception_ptr& failure){
      if(result)value=*result;
      if(failure){try{std::rethrow_exception(failure);}
        catch(const std::exception& cause){error=cause.what();}}
    });
    if(pendingBool)pendingBool->then([&](const bool* result,
                                           const std::exception_ptr& failure){
      if(result)value=*result;
      if(failure){try{std::rethrow_exception(failure);}
        catch(const std::exception& cause){error=cause.what();}}
    });
    loop.runReady();
    std::cout<<value::stringify(Json{{"before",before},
      {"after",Json{{"events",events},
        {"outbound",Json(outbound->snapshotQueued())}}},
      {"descriptor",descriptor},{"value",value},{"error",error}})<<'\n';
  }
}
