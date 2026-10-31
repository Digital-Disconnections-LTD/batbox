#include "instructions/hooks/runtime/CompactHooks.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;Json dispatch=Json::object();bool signalPresent=false;
  hook::CompactHookServices services;
  services.defaultTimeoutMs=600000;
  services.createBaseHookInput=[]{return hook::BaseHookInput{"s1","/tmp/t","/work"};};
  services.executeHooksOutsideRepl=[&](instructions::hooks::HookDispatch value,
    std::optional<std::stop_token> signal){
    dispatch={{"hookInput",value.hookInput},{"matchQuery",value.matchQuery},
      {"timeoutMs",value.timeoutMs}};
    signalPresent=signal.has_value();
    std::vector<hook::HookResult> rows;
    for(const auto& row:item.at("results"))
      rows.emplace_back(row.at("command").get<std::string>(),
        row.at("output").get<std::string>(),row.at("succeeded").get<bool>(),
        row.at("blocked").get<bool>());
    event_loop::Deferred<std::vector<hook::HookResult>> output(loop);
    output.resolve(std::move(rows));return output;
  };
  std::optional<std::stop_token> signal;
  if(item.value("signal",false))signal=std::stop_token{};
  std::optional<double> timeout;
  if(item.contains("timeout"))timeout=item.at("timeout").get<double>();
  hook::CompactHookOutput result;
  std::optional<std::string> custom;
  if(item.contains("customInstructions")&&!item.at("customInstructions").is_null())
    custom=item.at("customInstructions").get<std::string>();
  auto future=item.at("kind")=="pre"?
    hook::executePreCompactHooks(loop,services,item.at("trigger").get<std::string>(),
      custom,signal,timeout):
    hook::executePostCompactHooks(loop,services,item.at("trigger").get<std::string>(),
      item.at("summary").get<std::string>(),signal,timeout);
  future.then([&](const hook::CompactHookOutput* value,
    const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);result=*value;});
  loop.run();
  Json response=Json::object();
  if(result.newCustomInstructions)response["newCustomInstructions"]=
    *result.newCustomInstructions;
  if(result.userDisplayMessage)response["userDisplayMessage"]=
    *result.userDisplayMessage;
  return {{"name",item.at("name")},{"dispatch",dispatch},
    {"signalPresent",signalPresent},{"result",response},
    {"ownsNew",result.newCustomInstructionsOwnProperty},
    {"ownsDisplay",result.userDisplayMessageOwnProperty}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<value::stringify(rows)<<'\n';
}
