#include "settings/Sources.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
namespace settings=batbox::settings;
namespace value=batbox::value;
namespace {
Json strings(const std::vector<settings::Source>& values){
  Json result=Json::array();
  for(auto value:values)result.push_back(settings::sourceKey(value));
  return result;
}
Json run(const Json& input){
  const auto op=value::property(input,"op").get<std::string>();
  const auto source=value::property(input,"source");
  const auto value=source.is_string()?source.get<std::string>():std::string{};
  std::vector<settings::Source> allowed;
  const auto& rawAllowed=value::property(input,"allowed");
  if(rawAllowed.is_array())for(const auto& item:rawAllowed)
    allowed.push_back(settings::sourceFromKey(item.get<std::string>()));
  Json result;
  if(op=="constants")result={{"sources",strings(settings::settingSources())},
    {"editable",strings(settings::editableSources())}};
  else if(op=="name")result=settings::getSettingSourceName(
    settings::sourceFromKey(value));
  else if(op=="display")result=settings::getSourceDisplayName(value);
  else if(op=="lower")result=settings::getSettingSourceDisplayNameLowercase(value);
  else if(op=="capital")result=settings::getSettingSourceDisplayNameCapitalized(value);
  else if(op=="parse")result=strings(settings::parseSettingSourcesFlag(
    value::property(input,"flag").get<std::string>()));
  else if(op=="enabled")result=strings(settings::getEnabledSettingSources(allowed));
  else if(op=="isEnabled")result=settings::isSettingSourceEnabled(
    settings::sourceFromKey(value),allowed);
  else throw std::invalid_argument("Unknown operation");
  return {{"result",result}};
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
