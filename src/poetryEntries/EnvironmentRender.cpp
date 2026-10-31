#include "poetryEntries/Environment.hpp"
#include "poetryEntries/PoetryBlocks.hpp"
#include <stdexcept>

namespace batbox::poetryEntries {
namespace {
template<class Fn> void need(const Fn& function,const char* name){
  if(!function)throw std::invalid_argument(std::string("Missing environment binding: ")+name);
}
std::string join(const std::vector<std::string>& values,const char* separator){
  std::string result;
  for(std::size_t i=0;i<values.size();++i){
    if(i)result+=separator;
    result+=values[i];
  }
  return result;
}
std::optional<std::string> modelDescription(const std::string& model,
  const EnvironmentServices& services){
  if(services.internalUser&&services.undercover)return std::nullopt;
  need(services.marketingName,"marketingName");
  const auto marketing=services.marketingName(model);
  return marketing&&!marketing->empty()
    ?"You are powered by the model named "+*marketing+
       ". The exact model ID is "+model+"."
    :"You are powered by the model "+model+".";
}
std::pair<bool,std::string> gitAndUname(const EnvironmentServices& services){
  need(services.getIsGit,"getIsGit");
  auto isGit=services.getIsGit();
  const auto uname=getUnameSR(services);
  return {isGit.get(),uname};
}
}
std::string computeEnvInfo(const std::string& model,
  const std::optional<std::vector<std::string>>& additionalWorkingDirectories,
  const EnvironmentServices& services){
  const auto [isGit,uname]=gitAndUname(services);
  const auto description=modelDescription(model,services).value_or("");
  const auto additional=additionalWorkingDirectories&&
      !additionalWorkingDirectories->empty()
    ?"Additional working directories: "+
       join(*additionalWorkingDirectories,", ")+"\n":std::string{};
  const auto cutoff=getKnowledgeCutoff(model,services);
  const auto cutoffMessage=cutoff?"\n\nAssistant knowledge cutoff is "+*cutoff+".":
    std::string{};
  need(services.getCwd,"getCwd");
  return "Here is useful information about the environment you are running in:\n"
    "<env>\nWorking directory: "+services.getCwd()+
    "\nIs directory a git repo: "+(isGit?"Yes":"No")+"\n"+
    additional+"Platform: "+services.platform+"\n"+
    getShellInfoLine(services)+"\nOS Version: "+uname+
    "\n</env>\n"+description+cutoffMessage;
}
std::string computeSimpleEnvInfo(const std::string& model,
  const std::optional<std::vector<std::string>>& additionalWorkingDirectories,
  const EnvironmentServices& services){
  const auto [isGit,uname]=gitAndUname(services);
  const auto description=modelDescription(model,services);
  const auto cutoff=getKnowledgeCutoff(model,services);
  need(services.getCwd,"getCwd");
  const auto cwd=services.getCwd();
  const bool additional=additionalWorkingDirectories&&
    !additionalWorkingDirectories->empty();
  Json items=Json::array();
  items.push_back("Primary working directory: "+cwd);
  if(services.worktree)items.push_back(
    "This is a git worktree — an isolated copy of the repository. Run all "
    "commands from this directory. Do NOT `cd` to the original repository root.");
  items.push_back(Json::array({std::string("Is a git repository: ")+
    (isGit?"true":"false")}));
  if(additional){
    items.push_back("Additional working directories:");
    Json dirs=Json::array();
    for(const auto& dir:*additionalWorkingDirectories)dirs.push_back(dir);
    items.push_back(std::move(dirs));
  }
  items.push_back("Platform: "+services.platform);
  items.push_back(getShellInfoLine(services));
  items.push_back("OS Version: "+uname);
  if(description)items.push_back(*description);
  if(cutoff)items.push_back("Assistant knowledge cutoff is "+*cutoff+".");
  if(!(services.internalUser&&services.undercover)){
    if(services.productModelFamilyDescription.empty()||
       services.productAvailabilityDescription.empty()||
       services.fastModeDescription.empty())
      throw std::invalid_argument("Missing product model guidance binding");
    items.push_back(services.productModelFamilyDescription);
    items.push_back(services.productAvailabilityDescription);
    items.push_back(services.fastModeDescription);
  }
  const auto bullets=prependBullets(items);
  std::string result="# Environment\nYou have been invoked in the following environment: ";
  for(const auto& bullet:bullets)result+='\n'+bullet;
  return result;
}
}
