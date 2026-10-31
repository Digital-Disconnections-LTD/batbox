#include "cli/structured/ProcessLine.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <optional>
#include <string>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  std::string line;
  while(std::getline(std::cin,line)){
    const Json input=value::parse(line);
    Json events=Json::array(),environment=Json::object();
    cli::structured::ControlResponses control([](const Json&){});
    for(const auto& entry:input.value("pending",Json::array())){
      const auto id=entry.at("id").get<std::string>();
      cli::structured::PendingRequest pending;
      pending.request=Json{{"type","control_request"},{"request_id",id},
        {"request",Json{{"subtype",entry.at("subtype")},
          {"tool_use_id",entry.at("toolUseId")}}}};
      pending.resolve=[&,id](const Json& value){
        events.push_back(Json::array({"resolve",id,value}));
      };
      pending.reject=[&,id](const std::string& error){
        events.push_back(Json::array({"reject",id,error}));
      };
      const auto& schemaValue=value::property(entry,"schema");
      const auto schema=schemaValue.is_string()?schemaValue.get<std::string>():
        std::string();
      if(schema=="wrap")pending.parseResponse=[](const Json& value){
        return Json{{"wrapped",value}};
      };
      else if(schema=="throw")pending.parseResponse=[](const Json&)->Json{
        throw std::runtime_error("parse failed");
      };
      control.addPendingRequest(id,std::move(pending));
    }
    for(const auto& id:input.value("track",Json::array()))
      control.trackResolvedGadgetUseId(Json{{"request",Json{
        {"subtype","can_use_tool"},{"tool_use_id",id}}}});
    cli::structured::ProcessLineServices services;
    services.replayUserMessages=input.value("replay",false);
    services.setEnvironment=[&](const std::string& key,const std::string& value){
      environment[key]=value;
    };
    services.logForDebugging=[&](const std::string& text,const Json& options){
      events.push_back(Json::array({"debug",text,options}));
    };
    services.notifyCommandLifecycle=[&](const std::string& id,
                                          const std::string& state){
      events.push_back(Json::array({"lifecycle",id,state}));
    };
    services.onControlRequestResolved=[&](const std::string& id){
      events.push_back(Json::array({"resolved",id}));
    };
    if(input.value("unexpected",false))
      services.unexpectedResponseCallback=[&](const Json& message){
        events.push_back(Json::array({"unexpected",message}));
        event_loop::Deferred<event_loop::Unit> done(loop);
        done.resolve({});return done;
      };
    services.fatalInput=[](const std::string& text){
      throw std::runtime_error(text);
    };
    const auto pending=cli::structured::processLine(loop,
      input.at("line").get<std::string>(),control,services);
    const Json before=events;
    std::optional<std::optional<Json>> value;
    std::exception_ptr error;
    pending.then([&](const std::optional<Json>* result,const auto& failure){
      error=failure;if(result)value=*result;
    });
    loop.runReady();
    if(error)std::rethrow_exception(error);
    if(!value)throw std::runtime_error("processLine did not settle");
    Json resolved=Json::array();
    for(const auto& id:input.value("track",Json::array()))
      if(control.hasResolvedGadgetUseId(id.get<std::string>()))
        resolved.push_back(id);
    for(const auto& entry:input.value("pending",Json::array())){
      const auto& id=entry.at("toolUseId");
      if(control.hasResolvedGadgetUseId(id.get<std::string>())){
        bool exists=false;
        for(const auto& prior:resolved)if(prior==id){exists=true;break;}
        if(!exists)resolved.push_back(id);
      }
    }
    std::cout<<value::stringify(Json{{"before",before},{"after",events},
      {"value",*value?**value:Json()},{"environment",environment},
      {"pending",control.getPendingPermissionRequests()},
      {"resolved",resolved}})<<'\n';
  }
}
