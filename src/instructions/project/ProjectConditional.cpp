#include "instructions/project/ProjectDiscovery.hpp"
#include <filesystem>
#include <iterator>
#include <stdexcept>

namespace batbox::instructions::project {
namespace fs=std::filesystem;
namespace {
void append(std::vector<MemoryFileInfo>& to,std::vector<MemoryFileInfo> from){
  to.insert(to.end(),std::make_move_iterator(from.begin()),
    std::make_move_iterator(from.end()));
}
std::string join(const std::string& first,const std::string& second){
  return (fs::path(first)/second).string();
}
}
std::vector<MemoryFileInfo> processConditionedMdRules(
  const std::string& targetPath,const std::string& rulesDir,
  const std::string& type,PathSet& processedPaths,bool includeExternal,
  const DiscoveryServices& services){
  auto files=processMdRules(rulesDir,type,processedPaths,includeExternal,
    true,services);
  std::vector<MemoryFileInfo> result;
  for(auto& file:files){
    if(!file.globs||file.globs->empty())continue;
    const auto baseDir=type=="Project"
      ?fs::path(rulesDir).parent_path().parent_path().string()
      :(services.originalCwd?services.originalCwd():std::string{});
    if(baseDir.empty())throw std::invalid_argument("Missing instruction CWD binding");
    const auto relativePath=fs::path(targetPath).is_absolute()
      ?fs::path(targetPath).lexically_relative(baseDir).string():targetPath;
    if(relativePath.empty()||relativePath.starts_with("..")||
       fs::path(relativePath).is_absolute())continue;
    if(!services.matchRuleIgnore)
      throw std::invalid_argument("Missing ignore-rule matcher binding");
    if(services.matchRuleIgnore(relativePath,*file.globs))
      result.push_back(std::move(file));
  }
  return result;
}
std::vector<MemoryFileInfo> getManagedAndUserConditionalRules(
  const std::string& targetPath,PathSet& processedPaths,
  const DiscoveryServices& services){
  if(!services.managedRulesDir||!services.settingSourceEnabled)
    throw std::invalid_argument("Missing instruction rules/settings binding");
  std::vector<MemoryFileInfo> result;
  append(result,processConditionedMdRules(targetPath,services.managedRulesDir(),
    "Managed",processedPaths,false,services));
  if(services.settingSourceEnabled("userSettings")){
    if(!services.userRulesDir)
      throw std::invalid_argument("Missing user rules binding");
    append(result,processConditionedMdRules(targetPath,services.userRulesDir(),
      "User",processedPaths,true,services));
  }
  return result;
}
std::vector<MemoryFileInfo> getMemoryFilesForNestedDirectory(
  const std::string& dir,const std::string& targetPath,
  PathSet& processedPaths,const DiscoveryServices& services){
  if(!services.settingSourceEnabled)
    throw std::invalid_argument("Missing instruction settings binding");
  std::vector<MemoryFileInfo> result;
  if(services.settingSourceEnabled("projectSettings")){
    append(result,processMemoryFile(join(dir,"BATBOX.md"),"Project",
      processedPaths,false,services));
    append(result,processMemoryFile(join(dir,".batbox/BATBOX.md"),"Project",
      processedPaths,false,services));
  }
  if(services.settingSourceEnabled("localSettings"))
    append(result,processMemoryFile(join(dir,"BATBOX.local.md"),"Local",
      processedPaths,false,services));
  const auto rulesDir=join(dir,".batbox/rules");
  PathSet unconditionalProcessedPaths=processedPaths;
  append(result,processMdRules(rulesDir,"Project",unconditionalProcessedPaths,
    false,false,services));
  append(result,processConditionedMdRules(targetPath,rulesDir,"Project",
    processedPaths,false,services));
  processedPaths.insert(unconditionalProcessedPaths.begin(),
    unconditionalProcessedPaths.end());
  return result;
}
std::vector<MemoryFileInfo> getConditionalRulesForCwdLevelDirectory(
  const std::string& dir,const std::string& targetPath,
  PathSet& processedPaths,const DiscoveryServices& services){
  return processConditionedMdRules(targetPath,join(dir,".batbox/rules"),
    "Project",processedPaths,false,services);
}
}
