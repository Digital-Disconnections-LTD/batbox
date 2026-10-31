#include "instructions/project/ProjectDiscovery.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>

using batbox::Json;
namespace project=batbox::instructions::project;
namespace value=batbox::value;
namespace fs=std::filesystem;
namespace {
std::string str(const Json& obj,const char* key,const std::string& fallback={}){
  const auto& value=value::property(obj,key);
  return value.is_string()?value.get<std::string>():fallback;
}
bool flag(const Json& obj,const char* key){
  return value::truthy(value::property(obj,key));
}
Json strings(const project::PathSet& values){
  std::vector<std::string> sorted(values.begin(),values.end());
  std::sort(sorted.begin(),sorted.end());
  Json array=Json::array();for(const auto& value:sorted)array.push_back(value);
  return array;
}
bool inside(const std::string& path,const std::string& base){
  const auto relative=fs::path(path).lexically_relative(fs::path(base)).string();
  return relative.empty()||relative=="."||
    (relative!=".."&&!relative.starts_with("../")&&
     !fs::path(relative).is_absolute());
}
Json run(const Json& input){
  project::DiscoveryServices services;
  services.parse.paths.getCwd=[&]{return str(input,"cwd");};
  services.parse.paths.homeDirectory=[]{return std::string("/home/user");};
  services.parse.paths.platform=[]{return std::string("linux");};
  services.parse.paths.normalizeNFC=[](const std::string& value){return value;};
  services.originalCwd=[&]{return str(input,"cwd");};
  services.pathInWorkingPath=inside;
  services.excludePatterns=[&]{
    std::vector<std::string> out;
    const auto& value=value::property(input,"excludes");
    if(value.is_array())for(const auto& item:value)
      if(item.is_string())out.push_back(item.get<std::string>());
    return out;
  };
  services.matchExclude=[](const std::string& path,
    const std::vector<std::string>& patterns){
    return std::find(patterns.begin(),patterns.end(),path)!=patterns.end();
  };
  services.matchRuleIgnore=[](const std::string& path,
    const std::vector<std::string>& patterns){
    for(const auto& pattern:patterns){
      if(pattern.ends_with("/**")&&
         path.starts_with(pattern.substr(0,pattern.size()-3)+"/"))return true;
      if(pattern==path||path.starts_with(pattern+"/"))return true;
    }
    return false;
  };
  services.managedRulesDir=[&]{return str(input,"managedRules");};
  services.userRulesDir=[&]{return str(input,"userRules");};
  services.settingSourceEnabled=[&](const std::string& source){
    const auto& settings=value::property(input,"settings");
    if(settings.is_array())for(const auto& value:settings)if(value==source)return true;
    return false;
  };
  const auto type=str(input,"type","Project");
  const auto path=str(input,"path");
  project::PathSet processed,visited;
  const auto op=str(input,"op");
  if(op=="excluded")return {{"files",project::isBatboxMdExcluded(path,type,
    services)},{"processed",strings(processed)},{"visited",strings(visited)}};
  std::vector<project::MemoryFileInfo> files;
  if(op=="conditioned")files=project::processConditionedMdRules(
    str(input,"target"),path,type,processed,flag(input,"includeExternal"),services);
  else if(op=="nested")files=project::getMemoryFilesForNestedDirectory(
    str(input,"dir"),str(input,"target"),processed,services);
  else if(op=="cwdRules")files=project::getConditionalRulesForCwdLevelDirectory(
    str(input,"dir"),str(input,"target"),processed,services);
  else if(op=="managedUser")files=project::getManagedAndUserConditionalRules(
    str(input,"target"),processed,services);
  else if(op=="rules")files=project::processMdRules(path,type,processed,
    flag(input,"includeExternal"),flag(input,"conditionalRule"),services,
    &visited);
  else{
    std::optional<std::string> parent;
    if(input.contains("parent")&&input.at("parent").is_string())
      parent=str(input,"parent");
    files=project::processMemoryFile(path,type,processed,
      flag(input,"includeExternal"),services,
      input.contains("depth")?input.at("depth").get<int>():0,parent);
  }
  Json output=Json::array();
  for(const auto& file:files)output.push_back(project::memoryFileToJson(file));
  return {{"files",output},{"processed",strings(processed)},
    {"visited",strings(visited)}};
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
    catch(const std::exception& error)
      {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
  }
}
