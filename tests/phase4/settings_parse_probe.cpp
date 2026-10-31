#include "settings/Parse.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
namespace settings=batbox::settings;
namespace value=batbox::value;
namespace {
Json asJson(const settings::ParsedSettings& value){
  return {{"settings",value.settings},{"errors",value.errors}};
}
Json run(const Json& input){
  settings::Stash stash;
  settings::ParseServices services;
  services.filterInvalidPermissionRules=[](Json& data,const std::string& path){
    Json warnings=Json::array();
    if(!data.is_object()||!data.contains("permissions")||
       !data["permissions"].is_object()||
       !data["permissions"].contains("allow")||
       !data["permissions"]["allow"].is_array())return warnings;
    Json valid=Json::array();
    for(const auto& rule:data["permissions"]["allow"]){
      if(rule.is_string())valid.push_back(rule);
      else warnings.push_back({{"file",path},{"path","permissions.allow"},
        {"message","Non-string value in allow array was removed"},
        {"invalidValue",rule}});
    }
    data["permissions"]["allow"]=valid;
    return warnings;
  };
  services.validateSettingsSchema=[](const Json& data){
    return data.is_object()?settings::SchemaResult{true,data,Json::array()}
      :settings::SchemaResult{false,nullptr,Json::array()};
  };
  services.formatZodError=[](const Json&,const std::string& path){
    return Json::array({Json{{"file",path},{"path",""},
      {"message","Invalid or malformed JSON"}}});
  };
  const auto path=value::property(input,"path").get<std::string>();
  auto first=settings::parseSettingsFile(path,stash,services);
  Json results=Json::array();
  if(value::truthy(value::property(input,"repeat"))){
    if(first.settings.is_object())first.settings["__mutated"]=true;
    results.push_back(asJson(first));
    results.push_back(asJson(settings::parseSettingsFile(path,stash,services)));
  }else results.push_back(asJson(first));
  return {{"results",results}};
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
