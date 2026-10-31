#include "settings/Managed.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
namespace settings=batbox::settings;
namespace value=batbox::value;
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{
    const auto input=value::parse(line);
    const auto base=value::property(input,"base").get<std::string>();
    settings::PathServices pathServices;
    pathServices.processCwd="/";
    pathServices.originalCwd="/workspace";
    pathServices.configHome="/home/u/.batbox";
    pathServices.platform="linux";
    pathServices.managedPathOverride=base;
    pathServices.internalUser=true;
    settings::Paths paths(std::move(pathServices));
    settings::Stash stash;
    settings::ParseServices services;
    services.filterInvalidPermissionRules=[](Json&,const std::string&){
      return Json::array();};
    services.validateSettingsSchema=[](const Json& data){
      return data.is_object()?settings::SchemaResult{true,data,Json::array()}
        :settings::SchemaResult{false,nullptr,Json::array()};};
    services.formatZodError=[](const Json&,const std::string& path){
      return Json::array({Json{{"file",path},{"path",""},
        {"message","Invalid or malformed JSON"}}});};
    const auto loaded=settings::loadManagedFileSettings(paths,stash,services);
    const auto presence=settings::getManagedFileSettingsPresence(
      paths,stash,services);
    std::cout<<value::stringify({{"result",Json{
      {"loaded",Json{{"settings",loaded.settings},{"errors",loaded.errors}}},
      {"presence",Json{{"hasBase",presence.hasBase},
        {"hasDropIns",presence.hasDropIns}}}}}})<<'\n';
  }catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
