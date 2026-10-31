#include "instructions/project/async/AsyncDiscovery.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

using batbox::Json;
namespace project=batbox::instructions::project;
namespace async=batbox::instructions::project::async;
namespace value=batbox::value;
namespace fs=std::filesystem;
namespace {
std::string str(const Json& obj,const char* key,const std::string& fallback={}){
  const auto& value=value::property(obj,key);
  return value.is_string()?value.get<std::string>():fallback;
}
bool flag(const Json& obj,const char* key){return value::truthy(value::property(obj,key));}
Json strings(const project::PathSet& values){
  std::vector<std::string> sorted(values.begin(),values.end());std::sort(sorted.begin(),sorted.end());
  Json array=Json::array();for(const auto& value:sorted)array.push_back(value);return array;
}
bool inside(const std::string& path,const std::string& base){
  const auto relative=fs::path(path).lexically_relative(base).string();
  return relative.empty()||relative=="."||
    (relative!=".."&&!relative.starts_with("../")&&!fs::path(relative).is_absolute());
}
template<class T>T await(batbox::event_loop::Loop& loop,async::Deferred<T> promise){
  std::optional<T> result;std::exception_ptr error;
  promise.then([&](const T* value,const std::exception_ptr& failure){
    if(failure)error=failure;else result=*value;});
  loop.run();if(error)std::rethrow_exception(error);
  if(!result)throw std::runtime_error("Instruction operation pending");return *result;
}
Json run(const Json& input){
  batbox::event_loop::Loop loop;project::DiscoveryServices sync;
  sync.parse.paths.getCwd=[&]{return str(input,"cwd");};
  sync.parse.paths.homeDirectory=[]{return std::string("/home/user");};
  sync.parse.paths.platform=[]{return std::string("linux");};
  sync.parse.paths.normalizeNFC=[](const std::string& value){return value;};
  sync.originalCwd=[&]{return str(input,"cwd");};
  sync.pathInWorkingPath=inside;
  sync.excludePatterns=[&]{
    std::vector<std::string> out;
    const auto& value=value::property(input,"excludes");
    if(value.is_array())for(const auto& item:value)
      if(item.is_string())out.push_back(item.get<std::string>());
    return out;
  };
  sync.matchExclude=[](const std::string& path,
    const std::vector<std::string>& patterns){
    return std::find(patterns.begin(),patterns.end(),path)!=patterns.end();
  };
  sync.matchRuleIgnore=[](const std::string& path,
    const std::vector<std::string>& patterns){
    for(const auto& pattern:patterns){
      if(pattern.ends_with("/**")&&
         path.starts_with(pattern.substr(0,pattern.size()-3)+"/"))return true;
      if(pattern==path||path.starts_with(pattern+"/"))return true;
    }
    return false;
  };
  sync.managedRulesDir=[&]{return str(input,"managedRules");};
  sync.userRulesDir=[&]{return str(input,"userRules");};
  sync.settingSourceEnabled=[&](const std::string& source){
    const auto& settings=value::property(input,"settings");
    if(settings.is_array())for(const auto& value:settings)if(value==source)return true;
    return false;
  };
  auto services=async::indigenousAsyncFs(loop,sync);
  auto processed=std::make_shared<project::PathSet>();
  auto visited=std::make_shared<project::PathSet>();
  const auto type=str(input,"type","Project");
  const auto path=str(input,"path");
  const auto op=str(input,"op");
  if(op=="excluded")return {{"files",project::isBatboxMdExcluded(path,type,sync)},
    {"processed",strings(*processed)},{"visited",strings(*visited)}};
  async::MemoryFiles files;
  if(op=="conditioned")files=await(loop,async::processConditionedMdRules(loop,
    str(input,"target"),path,type,processed,flag(input,"includeExternal"),services));
  else if(op=="nested")files=await(loop,async::getMemoryFilesForNestedDirectory(loop,
    str(input,"dir"),str(input,"target"),processed,services));
  else if(op=="cwdRules")files=await(loop,async::getConditionalRulesForCwdLevelDirectory(
    loop,str(input,"dir"),str(input,"target"),processed,services));
  else if(op=="managedUser")files=await(loop,async::getManagedAndUserConditionalRules(
    loop,str(input,"target"),processed,services));
  else if(op=="rules")files=await(loop,async::processMdRules(loop,path,type,processed,
    flag(input,"includeExternal"),flag(input,"conditionalRule"),services,visited));
  else{
    std::optional<std::string> parent;
    if(input.contains("parent")&&input.at("parent").is_string())parent=str(input,"parent");
    files=await(loop,async::processMemoryFile(loop,path,type,processed,
      flag(input,"includeExternal"),services,
      input.contains("depth")?input.at("depth").get<int>():0,parent));
  }
  Json output=Json::array();for(const auto& file:files)output.push_back(project::memoryFileToJson(file));
  return {{"files",output},{"processed",strings(*processed)},
    {"visited",strings(*visited)}};
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
