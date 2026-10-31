#include "PoetryAsyncFixture.hpp"
#include "runtime/JsonValue.hpp"
#include <optional>
#include <iostream>

using batbox::Json;
namespace poetryEntries=batbox::poetryEntries;
namespace value=batbox::value;
namespace {
using namespace poetry_fixture;
Json run(const Json& input){
  Json events=Json::array(),steps=Json::array();
  batbox::event_loop::Loop loop;
  auto sectionStash=std::make_shared<poetryEntries::AsyncSectionStash>(loop);
  poetryEntries::AsyncSystemPoetryServices services;
  services.sections=sectionStash;
  poetry_fixture::bind(loop,services,input,events);
  const auto& sourceSteps=value::property(input,"steps");
  const auto& gadgets=value::property(input,"tools");
  const auto& clients=value::property(input,"mcpClients");
  std::optional<std::vector<std::string>> additional;
  if(input.contains("additional")&&input.at("additional").is_array()){
    additional=std::vector<std::string>{};
    for(const auto& item:input.at("additional"))
      if(item.is_string())additional->push_back(item.get<std::string>());
  }
  poetryEntries::AsyncSystemPoetryInput arguments;
  *arguments.gadgets=gadgets.is_array()?gadgets:Json::array();
  *arguments.mcpClients=clients.is_array()?clients:Json::array();
  *arguments.additionalWorkingDirectories=additional;
  arguments.model=str(input,"model","local");
  for(std::size_t i=0;i<sourceSteps.size();++i){
    try{
      const auto value=await(loop,poetryEntries::getSystemPoetryAsync(loop,arguments,services));
      Json result=Json::array();for(const auto& item:value)result.push_back(item);
      steps.push_back({{"result",result},{"events",events}});
    }catch(const std::exception& error){
      steps.push_back({{"error",error.what()},{"events",events}});
    }
    events=Json::array();
  }
  return {{"steps",steps}};
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
