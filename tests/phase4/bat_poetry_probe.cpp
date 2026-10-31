#include "poetryEntries/BatPoetry.hpp"
#include "runtime/JsonValue.hpp"
#include <future>
#include <iostream>

using batbox::Json;
namespace poetryEntries=batbox::poetryEntries;
namespace value=batbox::value;
namespace {
std::string str(const Json& value,const char* key,const std::string& fallback={}){
  const auto& item=value::property(value,key);
  return item.is_string()?item.get<std::string>():fallback;
}
bool flag(const Json& value,const char* key){return value::truthy(value::property(value,key));}
Json run(const Json& input){
  Json events=Json::array();
  if(str(input,"op")=="default")return {{"result",poetryEntries::defaultBatPoetry()},
    {"events",events}};
  poetryEntries::BatPoetryServices services;
  services.guidance.discoverSkillsFeature=flag(input,"feature");
  services.guidance.discoverSkillsGadgetName=str(input,"discoverName");
  services.skillSearchEnabled=[&]{events.push_back("skillSearch");
    return flag(input,"skillSearchEnabled");};
  services.computeEnvInfo=[&](const std::string&,
    const std::optional<std::vector<std::string>>&){
    events.push_back("env");
    std::promise<std::string> promise;
    promise.set_value(str(input,"envInfo","ENV"));
    return promise.get_future();};
  std::vector<std::string> existing;
  const auto& rawExisting=value::property(input,"existing");
  if(rawExisting.is_array())for(const auto& item:rawExisting)
    if(item.is_string())existing.push_back(item.get<std::string>());
  std::optional<std::vector<std::string>> additional;
  const auto& rawAdditional=value::property(input,"additional");
  if(rawAdditional.is_array()){
    additional=std::vector<std::string>{};
    for(const auto& item:rawAdditional)
      if(item.is_string())additional->push_back(item.get<std::string>());
  }
  std::optional<poetryEntries::GadgetNames> enabled;
  const auto& rawEnabled=value::property(input,"enabled");
  if(rawEnabled.is_array()){
    enabled=poetryEntries::GadgetNames{};
    for(const auto& item:rawEnabled)
      if(item.is_string())enabled->insert(item.get<std::string>());
  }
  const auto result=poetryEntries::enhanceSystemPoetryWithEnvDetails(existing,
    str(input,"model","local"),additional,enabled,services);
  Json array=Json::array();for(const auto& item:result)array.push_back(item);
  return {{"result",array},{"events",events}};
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
