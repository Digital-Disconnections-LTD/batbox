#include "settings/Loader.hpp"
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
    pathServices.originalCwd=base+"/project";
    pathServices.configHome=base+"/user";
    pathServices.flagSettingsPath=base+"/flag.json";
    pathServices.platform="linux";
    settings::Paths paths(std::move(pathServices));
    settings::Stash stash;
    settings::LoadServices services;
    services.paths=&paths;services.stash=&stash;
    const auto& allowed=value::property(input,"allowed");
    if(allowed.is_array())for(const auto& item:allowed)
      services.allowedSources.push_back(settings::sourceFromKey(
        item.get<std::string>()));
    settings::Loader loader(std::move(services));
    std::cout<<value::stringify({{"result",loader.rawSettingsContainsKey(
      value::property(input,"key").get<std::string>())}})<<'\n';
  }catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
