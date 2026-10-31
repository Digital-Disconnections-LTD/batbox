#include "cli/permissions/PoetryDecision.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <string>

using namespace batbox;
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const Json test=value::parse(line);
    Json events=Json::array();
    Json appState={{"toolPermissionContext",Json{{"mode","default"},
      {"rules",Json::array({"Read"})}}},{"other","retained"}};
    cli::permissions::PoetryDecisionServices services;
    services.setAppState=[&](const std::function<Json(const Json&)>& updater){
      events.push_back(Json::array({"setAppState"}));
      appState=updater(appState);
    };
    services.applyPermissionUpdates=[&](const Json& context,const Json& updates){
      events.push_back(Json::array({"apply",context,updates}));
      Json next=context;next["updates"]=updates;return next;
    };
    services.persistPermissionUpdates=[&](const Json& updates){
      events.push_back(Json::array({"persist",updates}));
    };
    services.logForDebugging=[&](const std::string& text){
      events.push_back(Json::array({"debug",text}));
    };
    services.abort=[&]{events.push_back(Json::array({"abort"}));};
    gadgets::Gadget gadget;gadget.name="Read";
    Json value,error;
    try{value=cli::permissions::permissionPoetryGadgetResultToPermissionDecision(
      test.at("result"),gadget,test.at("input"),services);}
    catch(const std::exception& failure){error=failure.what();}
    std::cout<<value::stringify(Json{{"events",events},{"appState",appState},
      {"value",value},{"error",error}})<<'\n';
  }
}
