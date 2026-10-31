#include "poetryEntries/Static.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
namespace poetryEntries=batbox::poetryEntries;
namespace value=batbox::value;
namespace {
std::string str(const Json& value,const char* key,const std::string& fallback={}){
  const auto& item=value::property(value,key);
  return item.is_string()?item.get<std::string>():fallback;
}
Json run(const Json& input){
  const auto op=str(input,"op");
  const bool internal=value::truthy(value::property(input,"internalUser"));
  if(op=="system")return {{"result",poetryEntries::getSimpleSystemSection()}};
  if(op=="doing")return {{"result",poetryEntries::getSimpleDoingMissionsSection(
    internal,str(input,"issues","Use /issue."),
    str(input,"askName","AskUserQuestion"))}};
  if(op=="actions")return {{"result",poetryEntries::getActionsSection()}};
  if(op=="tone")return {{"result",poetryEntries::getSimpleToneAndStyleSection(
    internal)}};
  return {{"result",poetryEntries::getOutputEfficiencySection(internal)}};
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
