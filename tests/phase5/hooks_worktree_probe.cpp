#include "instructions/hooks/runtime/WorktreeHooks.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
std::vector<hook::HookResult> resultsFor(const Json& item){
  std::vector<hook::HookResult> results;
  if(!item.contains("results"))return results;
  for(const auto& row:item.at("results"))
    results.emplace_back(row.at("command").get<std::string>(),
      row.at("output").get<std::string>(),row.at("succeeded").get<bool>());
  return results;
}
Json run(const Json& item){
  event_loop::Loop loop;Json trace=Json::array(),dispatch=nullptr,value=nullptr,error=nullptr;
  hook::WorktreeHookServices services;services.defaultTimeoutMs=600000;
  services.getHooksConfigFromSnapshot=[&]()->std::optional<Json>{
    trace.push_back("snapshot");return Json{{"WorktreeCreate",item.value("snapshotCreate",false)
      ?Json::array({Json::object()}):Json::array()},
      {"WorktreeRemove",item.value("snapshotRemove",false)
      ?Json::array({Json::object()}):Json::array()}};};
  services.getRegisteredHooks=[&]()->std::optional<Json>{
    trace.push_back("registered");return Json{{"WorktreeCreate",
      item.value("registeredCreate",Json::array())},
      {"WorktreeRemove",item.value("registeredRemove",false)
      ?Json::array({Json::object()}):Json::array()}};};
  services.shouldAllowManagedHooksOnly=[&]{trace.push_back("managed");
    return item.value("managed",false);};
  services.createBaseHookInput=[&]{trace.push_back("base");
    return hook::BaseHookInput{"s1","/tmp/t","/work"};};
  services.executeHooksOutsideRepl=[&](instructions::hooks::HookDispatch call){
    trace.push_back("execute");
    dispatch={{"hookInput",call.hookInput},{"timeoutMs",call.timeoutMs}};
    event_loop::Deferred<std::vector<hook::HookResult>> output(loop);
    if(item.contains("executionError"))output.reject(std::make_exception_ptr(
      std::runtime_error(item.at("executionError").get<std::string>())));
    else output.resolve(resultsFor(item));
    return output;};
  services.logForDebugging=[&](const std::string& text,const std::string& level){
    trace.push_back("log:"+level+":"+text);};
  const auto kind=item.at("kind").get<std::string>();
  if(kind=="presence"){
    try{value=hook::hasWorktreeCreateHook(services);}
    catch(const std::exception& failure){error=failure.what();}
  }else if(kind=="create"){
    hook::executeWorktreeCreateHook(loop,services,
      item.at("nameArg").get<std::string>()).then(
      [&](const std::string* result,const std::exception_ptr& failure){
        if(failure)try{std::rethrow_exception(failure);}
          catch(const std::exception& exception){error=exception.what();}
        else value={{"worktreePath",*result}};
      });
    loop.run();
  }else{
    hook::executeWorktreeRemoveHook(loop,services,
      item.at("path").get<std::string>()).then(
      [&](const bool* result,const std::exception_ptr& failure){
        if(failure)try{std::rethrow_exception(failure);}
          catch(const std::exception& exception){error=exception.what();}
        else value=*result;
      });
    loop.run();
  }
  return {{"name",item.at("name")},{"trace",trace},{"dispatch",dispatch},
    {"value",value},{"error",error}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<rows.dump()<<'\n';
}
