#include "poetryEntries/Environment.hpp"
#include "runtime/JsonValue.hpp"
#include <future>
#include <iostream>
#include <sys/utsname.h>

using batbox::Json;
namespace poetryEntries=batbox::poetryEntries;
namespace value=batbox::value;
namespace {
std::string str(const Json& value,const char* key,const std::string& fallback={}){
  const auto& item=value::property(value,key);
  return item.is_string()?item.get<std::string>():fallback;
}
bool flag(const Json& value,const char* key){return value::truthy(value::property(value,key));}
Json run(const Json& input){
  poetryEntries::EnvironmentServices services;
  services.environment=Json::object();
  if(input.contains("shell"))services.environment["SHELL"]=input.at("shell");
  services.platform=str(input,"platform","linux");
  services.internalUser=flag(input,"internalUser");
  services.undercover=flag(input,"undercover");
  services.worktree=flag(input,"worktree");
  services.getIsGit=[&]{std::promise<bool> value;
    value.set_value(flag(input,"isGit"));return value.get_future();};
  services.getCwd=[&]{return str(input,"cwd","/project");};
  services.marketingName=[&](const std::string&)->std::optional<std::string>{
    const auto name=str(input,"marketing");
    return name.empty()?std::nullopt:std::optional<std::string>(name);};
  services.canonicalModelName=[](const std::string& model){
    if(model=="premium_current")return std::string("premium-current");
    if(model=="standard_current")return std::string("standard-current");
    if(model=="fast_current")return std::string("fast-current");
    return model;};
  services.knowledgePatterns={{"standard-current","August 2025"},
    {"premium-current","May 2025"},
    {"fast-current","February 2025"}};
  struct utsname host{};::uname(&host);
  services.osVersion=[&]{return std::string(host.version);};
  services.osRelease=[&]{return std::string(host.release);};
  services.productModelFamilyDescription=
    "Batbox model set: premium, standard, fast.";
  services.productAvailabilityDescription=
    "Batbox is available in terminal, desktop, web, and IDE.";
  services.fastModeDescription=
    "Batbox fast mode uses the same premium model.";
  std::optional<std::vector<std::string>> additional;
  if(input.contains("additional")&&input.at("additional").is_array()){
    additional=std::vector<std::string>{};
    for(const auto& item:input.at("additional"))
      if(item.is_string())additional->push_back(item.get<std::string>());
  }
  const auto model=str(input,"model");
  const auto op=str(input,"op");
  if(op=="cutoff"){
    const auto value=poetryEntries::getKnowledgeCutoff(model,services);
    return {{"result",value?Json(*value):Json(nullptr)}};
  }
  if(op=="shell")return {{"result",poetryEntries::getShellInfoLine(services)}};
  if(op=="uname")return {{"result",poetryEntries::getUnameSR(services)}};
  if(op=="env")return {{"result",poetryEntries::computeEnvInfo(model,
    additional,services)}};
  return {{"result",poetryEntries::computeSimpleEnvInfo(model,
    additional,services)}};
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
