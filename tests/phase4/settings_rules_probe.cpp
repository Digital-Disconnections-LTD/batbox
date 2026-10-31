#include "settings/PermissionRules.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
namespace settings=batbox::settings;
namespace value=batbox::value;
namespace {
Json parsed(const settings::PermissionRuleValue& value){
  Json result={{"toolName",value.gadgetName}};
  if(value.ruleContent)result["ruleContent"]=*value.ruleContent;
  return result;
}
Json run(const Json& input){
  const settings::PermissionRuleServices services;
  const auto op=value::property(input,"op").get<std::string>();
  const auto& rule=value::property(input,"rule");
  const auto text=rule.is_string()?rule.get<std::string>():std::string{};
  Json result;
  if(op=="parse")result=parsed(settings::permissionRuleValueFromString(
    text,services));
  else if(op=="validate")result=settings::validationResultJson(
    settings::validatePermissionRule(text,services));
  else if(op=="filter"){
    auto data=value::property(input,"data");
    const auto& rawPath=value::property(input,"path");
    const auto path=rawPath.is_string()?rawPath.get<std::string>()
      :"settings.json";
    auto warnings=settings::filterInvalidPermissionRules(data,path,services);
    result={{"data",data},{"warnings",warnings}};
  }else if(op=="escape")result=settings::escapeRuleContent(
    value::property(input,"text").get<std::string>());
  else if(op=="unescape")result=settings::unescapeRuleContent(
    value::property(input,"text").get<std::string>());
  else if(op=="stringify"){
    const auto& value=value::property(input,"value");
    const auto& content=value::property(value,"ruleContent");
    result=settings::permissionRuleValueToString({
      value::property(value,"toolName").get<std::string>(),
      content.is_string()?std::optional<std::string>(content.get<std::string>())
        :std::nullopt});
  }else if(op=="mcp"){
    const auto info=settings::mcpInfoFromString(text);
    if(!info)result=nullptr;
    else{
      result={{"serverName",info->first}};
      if(info->second)result["toolName"]=*info->second;
    }
  }else if(op=="normalize")result=settings::normalizeLegacyGadgetName(
    text,services);
  else if(op=="legacy")result=settings::getLegacyGadgetNames(text,services);
  else throw std::invalid_argument("Unknown operation");
  return {{"result",result}};
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
