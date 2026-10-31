#include "instructions/project/ProjectDiscovery.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <iostream>

using batbox::Json;
namespace project=batbox::instructions::project;
namespace value=batbox::value;
namespace fs=std::filesystem;
namespace {
std::string str(const Json& obj,const char* key,const std::string& fallback={}){
  const auto& value=value::property(obj,key);
  return value.is_string()?value.get<std::string>():fallback;
}
bool flag(const Json& obj,const char* key){return value::truthy(value::property(obj,key));}
std::vector<project::MemoryFileInfo> files(const Json& input){
  std::vector<project::MemoryFileInfo> result;
  const auto& source=value::property(input,"files");
  if(!source.is_array())return result;
  for(const auto& raw:source){
    project::MemoryFileInfo file;
    file.path=str(raw,"path");file.type=str(raw,"type");
    file.content=str(raw,"content");
    if(raw.contains("parent")&&raw.at("parent").is_string())
      file.parent=str(raw,"parent");
    if(raw.contains("globs")&&raw.at("globs").is_array()){
      file.globs=std::vector<std::string>{};
      for(const auto& pattern:raw.at("globs"))
        if(pattern.is_string())file.globs->push_back(pattern.get<std::string>());
    }
    result.push_back(std::move(file));
  }
  return result;
}
Json jsonFiles(const std::vector<project::MemoryFileInfo>& files){
  Json result=Json::array();
  for(const auto& file:files){
    Json value={{"path",file.path},{"type",file.type},{"content",file.content}};
    if(file.parent)value["parent"]=*file.parent;
    if(file.globs){value["globs"]=Json::array();
      for(const auto& pattern:*file.globs)value["globs"].push_back(pattern);}
    result.push_back(value);
  }
  return result;
}
bool inside(const std::string& path,const std::string& base){
  const auto relative=fs::path(path).lexically_relative(base).string();
  return relative.empty()||relative=="."||
    (relative!=".."&&!relative.starts_with("../")&&
     !fs::path(relative).is_absolute());
}
Json run(const Json& input){
  project::DiscoveryServices services;
  services.originalCwd=[&]{return str(input,"cwd","/project");};
  services.pathInWorkingPath=inside;
  services.skipAutoMemoryIndex=[&]{return flag(input,"skipIndex");};
  services.skipProjectInstructions=[&]{return flag(input,"skipProject");};
  services.teamMemoryFeature=[&]{return flag(input,"teamFeature");};
  services.externalIncludesApproved=[&]{return flag(input,"approved");};
  services.externalIncludesWarningShown=[&]{return flag(input,"warningShown");};
  const auto list=files(input);
  const auto op=str(input,"op");
  if(op=="large")return {{"result",jsonFiles(project::getLargeMemoryFiles(list))}};
  if(op=="filter")return {{"result",jsonFiles(project::filterInjectedMemoryFiles(list,services))}};
  if(op=="prompt"){
    std::function<bool(const std::string&)> allowed;
    if(input.contains("allowed")&&input.at("allowed").is_array())
      allowed=[&](const std::string& type){
        for(const auto& value:input.at("allowed"))if(value==type)return true;
        return false;};
    return {{"result",project::getBatboxMds(list,services,allowed)}};
  }
  if(op=="external"||op=="hasExternal"){
    const auto externals=project::getExternalBatboxMdIncludes(list,services);
    if(op=="hasExternal")return {{"result",!externals.empty()}};
    Json result=Json::array();
    for(const auto& file:externals)
      result.push_back({{"path",file.path},{"parent",file.parent}});
    return {{"result",result}};
  }
  if(op=="warning"){
    project::MemoryDiscoveryStash stash;
    // This operation tests the approval short circuit; full forced discovery
    // is covered by project_hierarchy_probe.
    return {{"result",project::shouldShowBatboxMdExternalIncludesWarning(
      stash,services)}};
  }
  if(op=="isPath")return {{"result",project::isMemoryFilePath(str(input,"path"))}};
  if(op=="allPaths"){
    std::vector<std::string> keys;
    const auto& raw=value::property(input,"keys");
    if(raw.is_array())for(const auto& key:raw)
      if(key.is_string())keys.push_back(key.get<std::string>());
    Json result=Json::array();
    for(const auto& path:project::getAllMemoryFilePaths(list,keys))
      result.push_back(path);
    return {{"result",result}};
  }
  throw std::invalid_argument("Unknown test operation");
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
