#include "cli/headless/PermissionControl.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <string>

using namespace batbox;
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const Json input=value::parse(line);
    Json events=Json::array();
    cli::headless::PermissionControlServices services;
    services.isBypassPermissionsModeDisabled=[&]{
      events.push_back(Json::array({"disabled"}));
      return input.value("bypassDisabled",false);
    };
    services.transcriptClassifierEnabled=[&]{
      events.push_back(Json::array({"feature","TRANSCRIPT_CLASSIFIER"}));
      return input.value("classifier",false);
    };
    services.isAutoModeGateEnabled=[&]{
      events.push_back(Json::array({"gate"}));
      return input.value("gate",true);
    };
    services.getAutoModeUnavailableReason=[&]{
      events.push_back(Json::array({"reason"}));
      const auto& reason=value::property(input,"reason");
      return reason.is_string()?reason.get<std::string>():std::string();
    };
    services.getAutoModeUnavailableNotification=[&](const std::string& reason){
      events.push_back(Json::array({"notification",reason}));
      return "unavailable:"+reason;
    };
    services.transitionPermissionMode=[&](const std::string& from,
        const std::string& to,const Json& context){
      events.push_back(Json::array({"transition",from,to}));
      if(input.value("transitionError",false))
        throw std::runtime_error("transition failed");
      Json next=context;
      next["transitioned"]=true;
      return next;
    };
    services.enqueue=[&](const Json& value){
      events.push_back(Json::array({"enqueue",value}));
    };
    Json value,error;
    try{value=cli::headless::handleSetPermissionMode(
      Json{{"mode",input.at("mode")}},"request-1",input.at("context"),services);}
    catch(const std::exception& cause){error=cause.what();}
    std::cout<<value::stringify(Json{{"events",events},{"value",value},
      {"error",error}})<<'\n';
  }
}
