#include "skills/SkillGadgetValidation.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

namespace {
using batbox::Json;
std::vector<Json> values(const Json& input,const std::string& key){
  std::vector<Json> out;
  const auto& value=batbox::value::property(input,key);
  if(value.is_array())for(const auto& item:value)out.push_back(item);
  return out;
}
batbox::skills::RuleEntries rules(const Json& input,const std::string& key){
  batbox::skills::RuleEntries result;
  for(const auto& item:values(input,key))result.emplace_back(
    item.at("content").get<std::string>(),item.at("rule"));
  return result;
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    batbox::skills::SkillGadgetLookup lookup;
    lookup.findCommand=[](const std::string& name,const std::vector<Json>& commands){
      for(const auto& command:commands){
        if(batbox::value::property(command,"name")==name)return command;
        const auto& aliases=batbox::value::property(command,"aliases");
        if(aliases.is_array())for(const auto& alias:aliases)if(alias==name)return command;
      }
      return Json(nullptr);
    };
    const auto op=input.at("op").get<std::string>();
    Json result;
    const auto commands=batbox::skills::getAllSkillCommands(
      values(input,"local"),values(input,"mcp"));
    if(op=="all")result=commands;
    else if(op=="safe")result=batbox::skills::skillHasOnlySafeProperties(
      input.at("command"));
    else if(op=="validate"){
      const auto validation=batbox::skills::validateSkillInput(
        input.at("skill"),commands,lookup);
      result={{"result",validation.result}};
      if(validation.message)result["message"]=*validation.message;
      if(validation.errorCode)result["errorCode"]=*validation.errorCode;
    }else result=batbox::skills::checkSkillPermissions(input.at("input"),commands,
      rules(input,"deny"),rules(input,"allow"),lookup);
    std::cout<<batbox::value::stringify({{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}
