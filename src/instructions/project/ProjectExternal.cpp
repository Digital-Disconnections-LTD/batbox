#include "instructions/project/ProjectDiscovery.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include <stdexcept>
#include <unordered_set>

namespace batbox::instructions::project {
std::vector<ExternalInclude> getExternalBatboxMdIncludes(
  const std::vector<MemoryFileInfo>& files,const DiscoveryServices& services){
  if(!services.originalCwd||!services.pathInWorkingPath)
    throw std::invalid_argument("Missing instruction working-path binding");
  std::vector<ExternalInclude> result;
  for(const auto& file:files)
    if(file.type!="User"&&file.parent&&!file.parent->empty()&&
       !services.pathInWorkingPath(file.path,services.originalCwd()))
      result.push_back({file.path,*file.parent});
  return result;
}
bool hasExternalBatboxMdIncludes(const std::vector<MemoryFileInfo>& files,
  const DiscoveryServices& services){
  return !getExternalBatboxMdIncludes(files,services).empty();
}
bool shouldShowBatboxMdExternalIncludesWarning(MemoryDiscoveryStash& stash,
  const DiscoveryServices& services){
  if(!services.externalIncludesApproved||
     !services.externalIncludesWarningShown)
    throw std::invalid_argument("Missing external-include config binding");
  if(services.externalIncludesApproved()||
     services.externalIncludesWarningShown())return false;
  return hasExternalBatboxMdIncludes(stash.getMemoryFiles(true,services),
    services);
}
bool isMemoryFilePath(const std::string& path){
  std::string trimmed=path;
  const auto separator=static_cast<char>(std::filesystem::path::preferred_separator);
  const auto isSeparator=[separator](char value){
#if defined(_WIN32)
    if(value=='/')return true;
#endif
    return value==separator;
  };
  while(trimmed.size()>1&&isSeparator(trimmed.back()))
    trimmed.pop_back();
  const auto name=runtime::pathToUtf8(runtime::pathFromUtf8(trimmed).filename());
  if(name=="BATBOX.md"||name=="BATBOX.local.md")return true;
  return name.ends_with(".md")&&
    path.find(std::string(1,separator)+".batbox"+separator+
      "rules"+separator)!=std::string::npos;
}
std::vector<std::string> getAllMemoryFilePaths(
  const std::vector<MemoryFileInfo>& files,
  const std::vector<std::string>& readFileStateKeys){
  std::vector<std::string> paths;
  std::unordered_set<std::string> seen;
  for(const auto& file:files)
    if(!value::trim(file.content).empty()&&seen.insert(file.path).second)
      paths.push_back(file.path);
  for(const auto& path:readFileStateKeys)
    if(isMemoryFilePath(path)&&seen.insert(path).second)
      paths.push_back(path);
  return paths;
}
}
