#include "instructions/project/ProjectDiscovery.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <fstream>
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
bool contains(const Json& array,const std::string& value){
  if(!array.is_array())return false;
  for(const auto& item:array)if(item==value)return true;
  return false;
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
    std::vector<std::string> result;
    const auto& patterns=value::property(input,"excludes");
    if(patterns.is_array())for(const auto& pattern:patterns)
      if(pattern.is_string())result.push_back(pattern.get<std::string>());
    return result;
  };
  services.matchExclude=[](const std::string& path,
    const std::vector<std::string>& patterns){
    return std::find(patterns.begin(),patterns.end(),path)!=patterns.end();
  };
  services.externalIncludesApproved=[&]{return flag(input,"externalApproved");};
  services.externalIncludesWarningShown=[&]{return flag(input,"warningShown");};
  bool failMemoryPath=flag(input,"failMemoryPath");
  services.memoryPath=[&](const std::string& type){
    if(failMemoryPath)throw std::runtime_error("Injected memory-path failure");
    return str(input,type=="Managed"?"managed":"user");};
  services.managedRulesDir=[&]{return str(input,"managedRules");};
  services.userRulesDir=[&]{return str(input,"userRules");};
  services.settingSourceEnabled=[&](const std::string& source){
    return contains(value::property(input,"settings"),source);};
  services.findGitRoot=[&](const std::string&)->std::optional<std::string>{
    const auto path=str(input,"gitRoot");return path.empty()?std::nullopt:
      std::optional<std::string>(path);};
  services.findCanonicalGitRoot=[&](const std::string&)->std::optional<std::string>{
    const auto path=str(input,"canonicalRoot");return path.empty()?std::nullopt:
      std::optional<std::string>(path);};
  services.additionalDirectoriesEnabled=[&]{return flag(input,"additionalEnabled");};
  services.additionalDirectories=[&]{
    std::vector<std::string> result;
    const auto& dirs=value::property(input,"additional");
    if(dirs.is_array())for(const auto& dir:dirs)
      if(dir.is_string())result.push_back(dir.get<std::string>());
    return result;};
  services.autoMemoryEnabled=[&]{return flag(input,"autoEnabled");};
  services.autoMemoryEntrypoint=[&]{return str(input,"autoPath");};
  services.teamMemoryEnabled=[]{return false;};
  services.hasInstructionsLoadedHook=[&]{return flag(input,"hookEnabled");};
  Json events=Json::array();
  services.instructionsLoadedHook=[&](const project::MemoryFileInfo& file,
    const std::string& reason){
    Json meta=Json::object();
    if(file.globs){meta["globs"]=Json::array();
      for(const auto& pattern:*file.globs)meta["globs"].push_back(pattern);}
    if(file.parent)meta["parentFilePath"]=*file.parent;
    events.push_back({{"file",file.path},{"type",file.type},
      {"reason",reason},{"meta",meta}});
  };
  project::MemoryDiscoveryStash stash;
  Json steps=Json::array();
  for(const auto& step:value::property(input,"steps")){
    const auto op=str(step,"op");
    if(op=="clear"){stash.clearMemoryFileStashes();continue;}
    if(op=="repair"){failMemoryPath=false;continue;}
    if(op=="reset"){
      stash.resetGetMemoryFilesStash(str(step,"reason","session_start"));continue;
    }
    if(op=="write"){
      std::ofstream file(str(step,"path"),std::ios::binary|std::ios::trunc);
      file<<str(step,"content");continue;
    }
    if(op=="warning"){
      const bool warning=project::shouldShowBatboxMdExternalIncludesWarning(
        stash,services);
      steps.push_back({{"warning",warning},{"events",events}});
      events=Json::array();continue;
    }
    try{
      const auto files=stash.getMemoryFiles(flag(step,"force"),services);
      Json output=Json::array();
      for(const auto& file:files)output.push_back(project::memoryFileToJson(file));
      steps.push_back({{"files",output},{"events",events}});
      events=Json::array();
    }catch(const std::exception& error){
      steps.push_back({{"error",error.what()},{"events",events}});
      events=Json::array();
    }
  }
  return {{"steps",steps}};
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
