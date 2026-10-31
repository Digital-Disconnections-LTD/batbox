#include "instructions/hooks/runtime/FlightEndHooks.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;Json calls=Json::array(),dispatch=Json::object();
  hook::FlightEndHookServices services;
  services.defaultTimeoutMs=600000;
  services.createBaseHookInput=[&]{calls.push_back("base");
    return hook::BaseHookInput{"s1","/tmp/t","/work"};};
  services.executeHooksOutsideRepl=[&](instructions::hooks::HookDispatch value,
    const std::function<Json()>& getAppState,
    const std::optional<std::stop_token>& signal){
    dispatch={{"hookInput",value.hookInput},{"matchQuery",value.matchQuery},
      {"timeoutMs",value.timeoutMs},{"hasAppState",static_cast<bool>(getAppState)},
      {"hasSignal",signal.has_value()}};
    calls.push_back("execute");
    if(getAppState)getAppState();
    event_loop::Deferred<std::vector<hook::HookResult>> output(loop);
    if(item.contains("executionError"))output.reject(std::make_exception_ptr(
      std::runtime_error(item.at("executionError").get<std::string>())));
    else{
      std::vector<hook::HookResult> results;
      for(const auto& row:item.at("results"))
        results.emplace_back(row.at("command").get<std::string>(),
          row.at("output").get<std::string>(),row.at("succeeded").get<bool>(),
          row.at("blocked").get<bool>());
      output.resolve(std::move(results));
    }
    return output;};
  services.writeStderr=[&](const std::string& text){
    calls.push_back("stderr:"+text);
    if(item.contains("writeError"))
      throw std::runtime_error(item.at("writeError").get<std::string>());};
  services.getFlightId=[&]{calls.push_back("session-id");return "s1";};
  services.clearFlightHooks=[&](const hook::SetAppState&,
    const std::string& id){calls.push_back("clear:"+id);};
  hook::FlightEndOptions options;
  if(item.contains("timeout"))options.timeoutMs=item.at("timeout").get<double>();
  if(item.value("signal",false))options.signal=std::stop_token{};
  if(item.value("getAppState",false))options.getAppState=[&]{
    calls.push_back("get-state");return Json::object();};
  if(item.value("setAppState",false))options.setAppState=[](hook::AppStateUpdate){};
  Json error=nullptr;
  hook::executeFlightEndHooks(loop,services,item.at("reason").get<std::string>(),
    std::move(options)).then([&](const event_loop::Unit*,
    const std::exception_ptr& failure){
    if(failure){try{std::rethrow_exception(failure);}catch(const std::exception& e){
      error=e.what();}}
  });
  loop.run();
  return {{"name",item.at("name")},{"dispatch",dispatch},
    {"calls",calls},{"error",error}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<value::stringify(rows)<<'\n';
}
