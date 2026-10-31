#include "instructions/project/ProjectDiscovery.hpp"
#include <filesystem>
#include <iterator>
#include <stdexcept>

namespace batbox::instructions::project {
namespace fs=std::filesystem;
namespace {
template<class T> T required(const std::function<T()>& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Missing instruction binding: ")+name);
  return callback();
}
void append(std::vector<MemoryFileInfo>& to,std::vector<MemoryFileInfo> from){
  to.insert(to.end(),std::make_move_iterator(from.begin()),
    std::make_move_iterator(from.end()));
}
std::string join(const std::string& a,const std::string& b){
  return (fs::path(a)/b).string();
}
bool isInstructionType(const std::string& type){
  return type=="User"||type=="Project"||type=="Local"||type=="Managed";
}
}
std::vector<MemoryFileInfo> MemoryDiscoveryStash::getMemoryFiles(
  bool forceIncludeExternal,const DiscoveryServices& services){
  auto& slot=forceIncludeExternal?forced_:normal_;
  if(slot)return *slot;
  auto& errorSlot=forceIncludeExternal?forcedError_:normalError_;
  if(errorSlot)std::rethrow_exception(errorSlot);
  try{
  if(!services.memoryPath||!services.managedRulesDir||
     !services.userRulesDir||!services.settingSourceEnabled)
    throw std::invalid_argument("Missing instruction path/settings binding");
  std::vector<MemoryFileInfo> result;
  PathSet processed;
  const bool includeExternal=forceIncludeExternal||
    required(services.externalIncludesApproved,"externalIncludesApproved");
  append(result,processMemoryFile(services.memoryPath("Managed"),"Managed",
    processed,includeExternal,services));
  append(result,processMdRules(services.managedRulesDir(),"Managed",processed,
    includeExternal,false,services));
  if(services.settingSourceEnabled("userSettings")){
    append(result,processMemoryFile(services.memoryPath("User"),"User",
      processed,true,services));
    append(result,processMdRules(services.userRulesDir(),"User",processed,
      true,false,services));
  }
  const auto originalCwd=required(services.originalCwd,"originalCwd");
  std::vector<std::string> dirs;
  for(auto current=originalCwd;current!=fs::path(current).root_path().string();
      current=fs::path(current).parent_path().string()){
    dirs.push_back(current);
    if(current.empty()||fs::path(current).parent_path()==current)break;
  }
  const auto gitRoot=services.findGitRoot
    ?services.findGitRoot(originalCwd):std::nullopt;
  const auto canonicalRoot=services.findCanonicalGitRoot
    ?services.findCanonicalGitRoot(originalCwd):std::nullopt;
  bool nested=false;
  if(gitRoot&&canonicalRoot&&
     normalizePathForComparison(*gitRoot,services)!=
       normalizePathForComparison(*canonicalRoot,services)){
    if(!services.pathInWorkingPath)
      throw std::invalid_argument("Missing instruction working-path binding");
    nested=services.pathInWorkingPath(*gitRoot,*canonicalRoot);
  }
  for(auto dir=dirs.rbegin();dir!=dirs.rend();++dir){
    const bool skipProject=nested&&
      services.pathInWorkingPath(*dir,*canonicalRoot)&&
      !services.pathInWorkingPath(*dir,*gitRoot);
    if(services.settingSourceEnabled("projectSettings")&&!skipProject){
      append(result,processMemoryFile(join(*dir,"BATBOX.md"),"Project",
        processed,includeExternal,services));
      append(result,processMemoryFile(join(*dir,".batbox/BATBOX.md"),"Project",
        processed,includeExternal,services));
      append(result,processMdRules(join(*dir,".batbox/rules"),"Project",
        processed,includeExternal,false,services));
    }
    if(services.settingSourceEnabled("localSettings"))
      append(result,processMemoryFile(join(*dir,"BATBOX.local.md"),"Local",
        processed,includeExternal,services));
  }
  if(required(services.additionalDirectoriesEnabled,"additionalDirectoriesEnabled")){
    if(!services.additionalDirectories)
      throw std::invalid_argument("Missing additional-directories binding");
    for(const auto& dir:services.additionalDirectories()){
      append(result,processMemoryFile(join(dir,"BATBOX.md"),"Project",
        processed,includeExternal,services));
      append(result,processMemoryFile(join(dir,".batbox/BATBOX.md"),"Project",
        processed,includeExternal,services));
      append(result,processMdRules(join(dir,".batbox/rules"),"Project",
        processed,includeExternal,false,services));
    }
  }
  if(required(services.autoMemoryEnabled,"autoMemoryEnabled")){
    const auto path=required(services.autoMemoryEntrypoint,"autoMemoryEntrypoint");
    auto read=safelyReadMemoryFileAsync(path,"AutoMem",std::nullopt,services);
    if(read.info&&processed.insert(normalizePathForComparison(read.info->path,
         services)).second)result.push_back(std::move(*read.info));
  }
  if(required(services.teamMemoryEnabled,"teamMemoryEnabled")){
    const auto path=required(services.teamMemoryEntrypoint,"teamMemoryEntrypoint");
    auto read=safelyReadMemoryFileAsync(path,"TeamMem",std::nullopt,services);
    if(read.info&&processed.insert(normalizePathForComparison(read.info->path,
         services)).second)result.push_back(std::move(*read.info));
  }
  if(!forceIncludeExternal&&shouldFireHook_){
    shouldFireHook_=false;
    const auto reason=nextEagerLoadReason_;
    nextEagerLoadReason_="session_start";
    if(services.hasInstructionsLoadedHook&&
       services.hasInstructionsLoadedHook()){
      if(!services.instructionsLoadedHook)
        throw std::invalid_argument("Missing instructions-loaded hook binding");
      for(const auto& file:result)if(isInstructionType(file.type)){
        try{
          services.instructionsLoadedHook(file,file.parent?"include":reason);
        }catch(...){
          // Launch the async hook mission without awaiting it.
        }
      }
    }
  }
  slot=result;
  return result;
  }catch(...){
    // lodash memoize retains a rejected Promise until its stash is cleared.
    errorSlot=std::current_exception();
    throw;
  }
}
void MemoryDiscoveryStash::clearMemoryFileStashes(){
  normal_.reset();forced_.reset();
  normalError_=nullptr;forcedError_=nullptr;
}
void MemoryDiscoveryStash::resetGetMemoryFilesStash(const std::string& reason){
  nextEagerLoadReason_=reason;
  shouldFireHook_=true;
  clearMemoryFileStashes();
}
}
