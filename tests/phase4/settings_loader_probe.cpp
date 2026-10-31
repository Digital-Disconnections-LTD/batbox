#include "settings/Loader.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
namespace settings=batbox::settings;
namespace value=batbox::value;
namespace {
Json fixture(const Json& input,const char* key){
  const auto& value=value::property(input,key);
  return value.is_null()?Json{{"settings",Json::object()},
    {"errors",Json::array()}}:value;
}
settings::ParsedSettings parsed(const Json& value){
  return {value::property(value,"settings"),value::property(value,"errors")};
}
Json run(const Json& input){
  const auto base=value::property(input,"base").get<std::string>();
  settings::PathServices pathServices;
  pathServices.processCwd="/";
  pathServices.originalCwd=base+"/project";
  pathServices.configHome=base+"/user";
  pathServices.platform="linux";
  pathServices.flagSettingsPath=base+"/flag.json";
  pathServices.managedPathOverride=base+"/managed";
  pathServices.internalUser=true;
  settings::Paths paths(std::move(pathServices));
  settings::Stash stash;
  const auto& plugin=value::property(input,"plugin");
  if(!plugin.is_null())stash.setPluginSettingsBase(plugin);
  settings::LoadServices services;
  services.paths=&paths;services.stash=&stash;
  services.platform=value::property(input,"platform").is_string()
    ?value::property(input,"platform").get<std::string>():"linux";
  const auto& allowed=value::property(input,"allowed");
  if(allowed.is_array())for(const auto& item:allowed)
    services.allowedSources.push_back(settings::sourceFromKey(
      item.get<std::string>()));
  Json events=Json::array();
  services.remoteManagedSettings=[&]() -> std::optional<Json>{
    events.push_back("remote");
    const auto& value=value::property(input,"remote");
    return value.is_null()?std::nullopt:std::optional<Json>(value);
  };
  services.getMdmSettings=[&]{events.push_back("mdm");
    return parsed(fixture(input,"mdm"));};
  services.getHkcuSettings=[&]{events.push_back("hkcu");
    return parsed(fixture(input,"hkcu"));};
  services.flagSettingsInline=[&]() -> std::optional<Json>{
    events.push_back("inline");
    const auto& value=value::property(input,"inline");
    return value.is_null()?std::nullopt:std::optional<Json>(value);
  };
  services.parser.filterInvalidPermissionRules=[](Json&,
    const std::string&){return Json::array();};
  services.parser.validateSettingsSchema=[](const Json& data){
    return data.is_object()?settings::SchemaResult{true,data,Json::array()}
      :settings::SchemaResult{false,nullptr,Json::array()};};
  services.parser.formatZodError=[](const Json&,const std::string& path){
    return Json::array({Json{{"file",path},{"path",""},
      {"message","Invalid or malformed JSON"}}});};
  settings::Loader loader(std::move(services));
  Json outputs=Json::array();
  for(const auto& opJson:value::property(input,"ops")){
    const auto op=opJson.get<std::string>();
    if(op=="initial")outputs.push_back(loader.getInitialSettings());
    else if(op=="errors"){
      const auto value=loader.getSettingsWithErrors();
      outputs.push_back({{"settings",value.settings},
        {"errors",value.errors}});
    }else if(op=="sources"){
      const auto value=loader.getSettingsWithSources();
      outputs.push_back({{"effective",value.effective},
        {"sources",value.sources}});
    }else if(op=="origin"){
      const auto value=loader.getPolicySettingsOrigin();
      outputs.push_back(value?Json(*value):Json(nullptr));
    }else if(op.starts_with("source:"))outputs.push_back(
      loader.getSettingsForSource(settings::sourceFromKey(op.substr(7))));
    else throw std::invalid_argument("Unknown operation");
  }
  return {{"outputs",outputs},{"events",events}};
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
