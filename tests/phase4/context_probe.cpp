#include "context/Context.hpp"
#include "runtime/JsonValue.hpp"
#include <future>
#include <iostream>
#include <stdexcept>

using batbox::Json;
namespace context=batbox::context;
namespace project=batbox::instructions::project;
namespace value=batbox::value;
namespace {
template<class T> std::future<T> ready(T value){
  std::promise<T> promise;promise.set_value(std::move(value));
  return promise.get_future();
}
template<class T> std::future<T> failure(const std::string& message){
  std::promise<T> promise;
  promise.set_exception(std::make_exception_ptr(std::runtime_error(message)));
  return promise.get_future();
}
std::string str(const Json& obj,const char* key,const std::string& fallback={}){
  const auto& value=value::property(obj,key);
  return value.is_string()?value.get<std::string>():fallback;
}
bool flag(const Json& obj,const char* key){return value::truthy(value::property(obj,key));}
Json run(const Json& input){
  Json events=Json::array();
  context::Services services;
  services.testEnvironment=flag(input,"testEnv");
  services.environment=Json::object();
  for(const auto& pair:std::vector<std::pair<const char*,const char*>>{
      {"remote","BATBOX_REMOTE"},
      {"disableInstructions","BATBOX_DISABLE_BATBOX_MDS"},
      {"disableGitInstructions","BATBOX_DISABLE_GIT_INSTRUCTIONS"},
      {"dateOverride","BATBOX_OVERRIDE_DATE"}})
    if(input.contains(pair.first))
      services.environment[pair.second]=input.at(pair.first);
  Json initialSettings=Json::object();
  if(input.contains("includeGit"))
    initialSettings["includeGitInstructions"]=input.at("includeGit");
  services.includeGitInstructions=[&]{return context::shouldIncludeGitInstructions(
    services.environment,initialSettings);};
  services.getIsGit=[&]{events.push_back("getIsGit");
    return flag(input,"gitCheckError")?failure<bool>("git check failed"):
      ready(flag(input,"isGit"));};
  services.getBranch=[&]{events.push_back("getBranch");
    return flag(input,"branchError")?failure<std::string>("branch failed"):
      ready(str(input,"branch"));};
  services.getDefaultBranch=[&]{events.push_back("getDefaultBranch");
    return ready(str(input,"defaultBranch"));};
  services.gitStdoutNoThrow=[&](const std::vector<std::string>& args){
    std::string command="exec:";
    for(std::size_t i=0;i<args.size();++i){if(i)command+=' ';command+=args[i];}
    events.push_back(command);
    const auto key=args.size()>1&&args[1]=="status"?"status":
      args.size()>1&&args[1]=="log"?"log":"userName";
    return ready(str(input,key));
  };
  services.logError=[&](std::exception_ptr){events.push_back("logError");};
  services.stashBreakerFeature=[&]{return flag(input,"cacheBreaker");};
  services.bareMode=[&]{return flag(input,"bare");};
  services.additionalInstructionDirectories=[&]{
    std::vector<std::string> dirs;
    const auto& source=value::property(input,"additional");
    if(source.is_array())for(const auto& item:source)
      if(item.is_string())dirs.push_back(item.get<std::string>());
    return dirs;
  };
  services.getMemoryFiles=[&]{
    events.push_back("getMemoryFiles");
    context::MemoryFiles files;
    const auto& raw=value::property(input,"files");
    if(raw.is_array())for(const auto& item:raw){
      project::MemoryFileInfo file;
      file.path=str(item,"path");file.type=str(item,"type");
      file.content=str(item,"content");
      files.push_back(std::move(file));
    }
    return files;
  };
  project::DiscoveryServices projectServices;
  projectServices.skipAutoMemoryIndex=[&]{return flag(input,"skipIndex");};
  projectServices.skipProjectInstructions=[&]{return flag(input,"skipProject");};
  projectServices.teamMemoryFeature=[]{return false;};
  services.projectServices=&projectServices;
  services.setStashedBatboxMdContent=[&](const std::optional<std::string>& value){
    events.push_back({{"cachedContent",value?Json(*value):Json(nullptr)}});
  };
  services.localISODate=[&]{
    const auto override=str(input,"dateOverride");
    return context::getLocalISODate(override.empty()?std::nullopt:
      std::optional<std::string>(override));
  };
  context::ContextStash stash;
  Json steps=Json::array();
  for(const auto& step:value::property(input,"steps")){
    const auto op=str(step,"op");
    if(op=="clearGit"){stash.clearGitStatusStash();continue;}
    if(op=="clearSystem"){stash.clearSystemContextStash();continue;}
    if(op=="clearUser"){stash.clearUserContextStash();continue;}
    if(op=="inject"){
      std::optional<std::string> value;
      if(step.contains("value")&&step.at("value").is_string())
        value=str(step,"value");
      stash.setSystemPoetryInjection(value);
      steps.push_back({{"injection",stash.getSystemPoetryInjection()
        ?Json(*stash.getSystemPoetryInjection()):Json(nullptr)},
        {"events",events}});
      events=Json::array();continue;
    }
    try{
      Json result;
      if(op=="git"){
        const auto value=stash.getGitStatus(services);
        result=value?Json(*value):Json(nullptr);
      }else if(op=="system")result=stash.getSystemContext(services);
      else if(op=="date")result=services.localISODate();
      else result=stash.getUserContext(services);
      steps.push_back({{"result",result},{"events",events}});
    }catch(const std::exception& error){
      steps.push_back({{"error",error.what()},{"events",events}});
    }
    events=Json::array();
  }
  return {{"steps",steps}};
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
