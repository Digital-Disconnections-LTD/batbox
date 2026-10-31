#include "instructions/hooks/runtime/EnvHooks.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;Json calls=Json::array();
  hook::EnvHookServices services;
  services.defaultTimeoutMs=600000;
  services.createBaseHookInput=[&]{calls.push_back("base");
    return hook::BaseHookInput{"s1","/tmp/t","/work"};};
  services.executeHooksOutsideRepl=[&](instructions::hooks::HookDispatch dispatch){
    calls.push_back("execute:"+value::stringify(Json{
      {"hookInput",dispatch.hookInput},{"timeoutMs",dispatch.timeoutMs}}));
    std::vector<hook::HookResult> results;
    for(const auto& row:item.at("results")){
      std::optional<std::vector<std::string>> paths;
      if(row.contains("watchPaths"))
        paths=row.at("watchPaths").get<std::vector<std::string>>();
      std::optional<std::string> message;
      if(row.contains("systemMessage"))
        message=row.at("systemMessage").get<std::string>();
      results.emplace_back(row.at("command").get<std::string>(),
        row.at("output").get<std::string>(),row.at("succeeded").get<bool>(),
        row.at("blocked").get<bool>(),paths,message);
    }
    event_loop::Deferred<std::vector<hook::HookResult>> output(loop);
    output.resolve(std::move(results));return output;
  };
  services.invalidateFlightEnvStash=[&]{calls.push_back("invalidate");
    if(item.contains("invalidateError"))
      throw std::runtime_error(item.at("invalidateError").get<std::string>());};
  std::optional<double> timeout;
  if(item.contains("timeout"))timeout=item.at("timeout").get<double>();
  event_loop::Deferred<hook::EnvHookResult> output=
    item.at("kind")=="cwd"?
      hook::executeCwdChangedHooks(loop,services,"/old","/new",timeout):
      hook::executeFileChangedHooks(loop,services,"/work/file",
        item.at("event").get<std::string>(),timeout);
  Json result=Json::object();
  output.then([&](const hook::EnvHookResult* value,
    const std::exception_ptr& error){
    if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){
      result["error"]=e.what();}return;}
    Json rows=Json::array();for(const auto& row:value->results)
      rows.push_back(row.toJson());
    result["result"]={{"results",rows},{"watchPaths",value->watchPaths},
      {"systemMessages",value->systemMessages}};
  });
  loop.run();
  result["name"]=item.at("name");result["calls"]=calls;
  return result;
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<value::stringify(rows)<<'\n';
}
