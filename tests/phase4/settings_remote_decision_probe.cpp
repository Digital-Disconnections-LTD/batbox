#include "settings/remote/Security.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  using namespace batbox::settings::remote;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop loop;
    Json trace=Json::array();
    SecurityServices services;
    services.config={{"apiKeyHelper","awsAuthRefresh"},{"BATBOX_SAFE"}};
    services.isInteractive=[&](){trace.push_back({"interactive"});
      return test.value("interactive",true);};
    services.showDialog=[&](const Json& settings){
      trace.push_back({"dialog",settings});
      event_loop::Deferred<SecurityDecision> done(loop);
      done.resolve(test.value("decision",std::string{})=="rejected"
        ?SecurityDecision::Rejected:SecurityDecision::Approved);
      return done;
    };
    services.gracefulShutdown=[&](int code){
      trace.push_back({"shutdown",code});};
    const auto old=test.contains("old")&&!test["old"].is_null()
      ?std::optional(test["old"]):std::nullopt;
    const auto next=test.contains("settings")&&!test["settings"].is_null()
      ?std::optional(test["settings"]):std::nullopt;
    bool done=false;
    Json result;
    checkManagedSettingsSecurity(loop,old,next,services).then([&](
        const SecurityDecision* value,const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);
      const auto name=*value==SecurityDecision::Rejected?"rejected":
        *value==SecurityDecision::Approved?"approved":"no_check_needed";
      result=Json{{"result",name},
        {"continueRunning",handleSecurityCheckResult(*value,services)}};
      done=true;
    });
    for(int i=0;i<100&&!done;i++)loop.runReady();
    if(!done)throw std::runtime_error("Security decision did not settle");
    result["trace"]=trace;
    std::cout<<value::stringify(result)<<'\n';
  }
}
