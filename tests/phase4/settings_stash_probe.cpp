#include "settings/Stash.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
namespace settings=batbox::settings;
namespace value=batbox::value;
namespace {
Json presence(const std::optional<Json>& value){
  return {{"present",value.has_value()},
    {"value",value.value_or(Json(nullptr))}};
}
Json run(const Json& input){
  settings::Stash stash;
  Json outputs=Json::array();
  for(const auto& step:value::property(input,"steps")){
    const auto op=value::property(step,"op").get<std::string>();
    if(op=="getSession")outputs.push_back(
      stash.getFlightSettingsStash().value_or(Json(nullptr)));
    else if(op=="setSession")stash.setFlightSettingsStash(
      value::property(step,"value"));
    else if(op=="getSource")outputs.push_back(presence(
      stash.getStashedSettingsForSource(settings::sourceFromKey(
        value::property(step,"source").get<std::string>()))));
    else if(op=="setSource")stash.setStashedSettingsForSource(
      settings::sourceFromKey(value::property(step,"source").get<std::string>()),
      value::property(step,"value"));
    else if(op=="getParsed"){
      const auto value=stash.getStashedParsedFile(
        value::property(step,"path").get<std::string>());
      outputs.push_back({{"present",value.has_value()},
        {"value",value?Json{{"settings",value->settings},
          {"errors",value->errors}}:Json(nullptr)}});
    }else if(op=="setParsed"){
      const auto& value=value::property(step,"value");
      stash.setStashedParsedFile(value::property(step,"path").get<std::string>(),
        {value::property(value,"settings"),value::property(value,"errors")});
    }else if(op=="reset")stash.resetSettingsStash();
    else if(op=="getPlugin")outputs.push_back(presence(
      stash.getPluginSettingsBase()));
    else if(op=="setPlugin")stash.setPluginSettingsBase(
      value::property(step,"value"));
    else if(op=="clearPlugin")stash.clearPluginSettingsBase();
    else throw std::invalid_argument("Unknown operation");
  }
  return {{"outputs",outputs}};
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
