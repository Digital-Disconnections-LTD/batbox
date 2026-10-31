#include "instructions/hooks/runtime/CommandLeaves.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;Json trace=Json::array(),command=nullptr,value=nullptr,error=nullptr;
  hook::CommandLeafServices services;
  services.shouldDisableAllHooksIncludingManaged=[&]{trace.push_back("disabled");
    return item.value("disabled",false);};
  services.shouldSkipHookDueToTrust=[&]{trace.push_back("trust");
    return item.value("trust",false);};
  services.shouldAllowManagedHooksOnly=[&]{trace.push_back("managed");
    return item.value("managed",false);};
  services.getPolicySettings=[&]()->std::optional<Json>{
    trace.push_back("policy:policySettings");
    return item.contains("policy")?std::optional<Json>(item.at("policy")):std::nullopt;};
  services.getSettings=[&]()->std::optional<Json>{trace.push_back("settings");
    return item.contains("settings")?std::optional<Json>(item.at("settings")):std::nullopt;};
  services.randomUUID=[&]{trace.push_back("uuid");return "uuid-1";};
  services.logForDebugging=[&](const std::string& message,const std::string& level){
    trace.push_back("log:"+level+":"+message);};
  services.stringifyThrown=[](std::exception_ptr failure){
    try{std::rethrow_exception(failure);}
    catch(const std::exception& exception){return "Error: "+std::string(exception.what());}
    return std::string("Error");};
  services.execCommandHook=[&](hook::CommandRequest request){
    trace.push_back("execute");
    command={{"hook",request.hook},{"event",request.hookEvent},
      {"name",request.hookName},{"jsonInput",request.jsonInput},
      {"uuid",request.uuid},{"hasSignal",request.signal.stop_possible()}};
    event_loop::Deferred<hook::CommandOutcome> result(loop);
    if(item.contains("executionError"))result.reject(std::make_exception_ptr(
      std::runtime_error(item.at("executionError").get<std::string>())));
    else{
      const auto& row=item.at("result");
      hook::CommandOutcome outcome;
      outcome.status=row.at("status").get<int>();
      outcome.stdoutText=row.at("stdout").get<std::string>();
      outcome.aborted=row.at("aborted").get<bool>();
      result.resolve(std::move(outcome));
    }
    return result;};
  const auto kind=item.at("kind").get<std::string>();
  if(kind=="status"){
    hook::executeStatusLineCommand(loop,services,
      Json{{"cwd","/work"},{"model","batbox"}},std::nullopt,5000,
      item.value("logResult",false)).then(
      [&](const std::optional<std::string>* result,
        const std::exception_ptr& failure){
        if(failure)try{std::rethrow_exception(failure);}
          catch(const std::exception& exception){error=exception.what();}
        else if(*result)value=**result;
      });
  }else{
    hook::executeFileSuggestionCommand(loop,services,
      Json{{"cwd","/work"},{"prefix","a"}}).then(
      [&](const std::vector<std::string>* result,
        const std::exception_ptr& failure){
        if(failure)try{std::rethrow_exception(failure);}
          catch(const std::exception& exception){error=exception.what();}
        else value=*result;
      });
  }
  loop.run();
  return {{"name",item.at("name")},{"trace",trace},{"command",command},
    {"value",value},{"error",error}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<rows.dump()<<'\n';
}
