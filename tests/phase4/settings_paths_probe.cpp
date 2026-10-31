#include "settings/Paths.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
namespace settings=batbox::settings;
namespace value=batbox::value;
namespace {
std::string get(const Json& value,const char* key,const std::string& fallback={}){
  const auto& item=value::property(value,key);
  return item.is_string()?item.get<std::string>():fallback;
}
bool flag(const Json& value,const char* key){
  return value::truthy(value::property(value,key));
}
Json run(const Json& input){
  settings::PathServices services;
  services.processCwd=get(input,"processCwd");
  services.originalCwd=get(input,"originalCwd","/workspace");
  services.configHome=get(input,"configHome","/home/test/.batbox");
  services.platform=get(input,"platform","linux");
  const auto flagPath=get(input,"flagPath");
  if(!flagPath.empty())services.flagSettingsPath=flagPath;
  const auto managed=get(input,"managedOverride");
  if(!managed.empty())services.managedPathOverride=managed;
  services.internalUser=flag(input,"internalUser");
  services.coworkPlugins=flag(input,"cowork");
  services.coworkEnvironment=flag(input,"coworkEnv");
  settings::Paths paths(std::move(services));
  Json result={{"managed",paths.getManagedFilePath()},
    {"dropIn",paths.getManagedSettingsDropInDir()},
    {"managedFile",paths.getManagedSettingsFilePath()},
    {"userFile",paths.getUserSettingsFilePath()},
    {"paths",Json::object()},{"roots",Json::object()},
    {"relative",Json::object()}};
  for(auto source:settings::settingSources()){
    const auto key=settings::sourceKey(source);
    const auto file=paths.getSettingsFilePathForSource(source);
    result["paths"][key]=file?Json(*file):Json(nullptr);
    result["roots"][key]=paths.getSettingsRootPathForSource(source);
    if(source==settings::Source::Project||source==settings::Source::Local)
      result["relative"][key]=paths.getRelativeSettingsFilePathForSource(source);
  }
  return {{"result",result}};
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
