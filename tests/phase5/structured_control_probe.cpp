#include "cli/structured/ControlResponses.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <string>

using namespace batbox;
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const Json input=value::parse(line);
    Json events=Json::array();
    cli::structured::ControlResponses control([&](const Json& message){
      events.push_back(Json::array({"write",message}));
    });
    const auto makeRequest=[](const Json& entry){
      return Json{{"type","control_request"},
        {"request_id",entry.at("id")},
        {"request",Json{{"subtype",entry.at("subtype")},
          {"tool_use_id",entry.at("toolUseId")}}}};
    };
    for(const auto& entry:input.value("pending",Json::array())){
      const auto id=entry.at("id").get<std::string>();
      cli::structured::PendingRequest pending;
      pending.request=makeRequest(entry);
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
      control.trackResolvedGadgetUseId(makeRequest(Json{{"id","track"},
        {"subtype","can_use_tool"},{"toolUseId",id}}));
    const Json before=control.getPendingPermissionRequests();
    for(const auto& response:input.value("inject",Json::array()))
      control.injectControlResponse(response);
    const Json after=control.getPendingPermissionRequests();
    Json checks=Json::array();
    for(const auto& id:input.value("check",Json::array()))
      checks.push_back(control.hasResolvedGadgetUseId(id.get<std::string>()));
    std::cout<<value::stringify(Json{{"before",before},{"after",after},
      {"events",events},{"resolvedSize",control.resolvedGadgetUseCount()},
      {"has",checks}})<<'\n';
  }
}
