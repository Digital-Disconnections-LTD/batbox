#include "instructions/hooks/runtime/IfCondition.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;Json calls=Json::array();
  gadgets::Gadget gadget;gadget.name="Bash";
  gadget.parseInput=[&](const Json& value){
    calls.push_back("parse");
    if(!item.value("valid",false))throw std::runtime_error("invalid input");
    return value;
  };
  if(item.value("pattern",false))
    gadget.preparePermissionMatcher=[&](const Json&){
      calls.push_back("prepare");
      std::promise<std::function<bool(const std::string&)>> pending;
      pending.set_value([](const std::string& content){return content=="git *";});
      return pending.get_future();
    };
  gadgets::Gadgets available;
  if(item.value("hasTool",false))available.push_back(&gadget);
  auto prepare=hook::indigenousPrepareIfConditionMatcher(loop,available,{});
  Json input={{"hook_event_name",item.at("event")},
    {"tool_name",item.at("toolName")},
    {"tool_input",{{"command","git status"}}}};
  Json results=nullptr;
  prepare(input).then([&](const std::optional<hook::IfConditionMatcher>* matcher,
    const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    if(!*matcher)return;
    results=Json::array();
    for(const auto& condition:item.at("conditions"))
      results.push_back((**matcher)(condition.get<std::string>()));
  });
  loop.run();
  return {{"name",item.at("name")},{"results",results},{"calls",calls}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream stream(argv[1]);Json cases;stream>>cases;Json output=Json::array();
  for(const auto& item:cases)output.push_back(run(item));
  std::cout<<value::stringify(output)<<'\n';
}
