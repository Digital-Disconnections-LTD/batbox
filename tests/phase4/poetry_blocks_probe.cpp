#include "poetryEntries/PoetryBlocks.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
namespace poetryEntries=batbox::poetryEntries;
namespace value=batbox::value;
namespace {
Json array(const std::vector<std::string>& values){
  Json result=Json::array();for(const auto& value:values)result.push_back(value);
  return result;
}
Json optional(const std::optional<std::string>& value){
  return value?Json(*value):Json(nullptr);
}
Json run(const Json& input){
  const auto& operation=value::property(input,"op");
  if(operation=="bullets")return {{"result",array(poetryEntries::prependBullets(
    value::property(input,"items")))}};
  if(operation=="language"){
    const auto& raw=value::property(input,"language");
    const std::optional<std::string> language=raw.is_string()
      ?std::optional<std::string>(raw.get<std::string>()):std::nullopt;
    return {{"result",optional(poetryEntries::getLanguageSection(language))}};
  }
  if(operation=="style")return {{"result",optional(
    poetryEntries::getOutputStyleSection(value::property(input,"config")))}};
  if(operation=="mcp")return {{"result",optional(
    poetryEntries::getMcpInstructions(value::property(input,"clients")))}};
  if(operation=="mcpSection")return {{"result",optional(
    poetryEntries::getMcpInstructionsSection(value::property(input,"clients")))}};
  if(operation=="intro")return {{"result",poetryEntries::getSimpleIntroSection(
    value::property(input,"config"),"CYBER INSTRUCTION")}};
  throw std::invalid_argument("Unknown operation");
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
