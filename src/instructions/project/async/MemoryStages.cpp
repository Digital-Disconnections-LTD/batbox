#include "instructions/project/async/MemoryRun.hpp"
#include "shell/PosixPath.hpp"
#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace batbox::instructions::project::async {
namespace {
std::string join(const std::string& left,const std::string& right){
  return shell::joinPosixPath(left,right);
}
std::string dotFile(const std::string& dir){return join(join(dir,".batbox"),"BATBOX.md");}
std::string rulesDir(const std::string& dir){return join(join(dir,".batbox"),"rules");}
}
void MemoryRun::setupDirs(){
  try{
    if(!services.sync.originalCwd||!services.sync.findGitRoot||
      !services.sync.findCanonicalGitRoot)
      throw std::invalid_argument("Instruction cwd/Git root imports required");
    originalCwd=services.sync.originalCwd();
    auto current=originalCwd;
    while(current!=std::filesystem::path(current).root_path().string()){
      dirs.push_back(current);
      const auto parent=std::filesystem::path(current).parent_path().string();
      if(parent==current||parent.empty())break;
      current=parent;
    }
    gitRoot=services.sync.findGitRoot(originalCwd);
    canonicalRoot=services.sync.findCanonicalGitRoot(originalCwd);
    if(gitRoot&&canonicalRoot&&
      normalizePathForComparison(*gitRoot,services.sync)!=
      normalizePathForComparison(*canonicalRoot,services.sync)){
      if(!services.sync.pathInWorkingPath)
        throw std::invalid_argument("Instruction working-path import required");
      nestedWorktree=services.sync.pathInWorkingPath(*gitRoot,*canonicalRoot);
    }
    std::reverse(dirs.begin(),dirs.end());
    nextDir();
  }catch(...){fail(std::current_exception());}
}
void MemoryRun::nextDir(){
  try{
    while(dirIndex<dirs.size()){
      currentDir=dirs[dirIndex++];
      bool skipProject=false;
      if(nestedWorktree){
        if(!services.sync.pathInWorkingPath)
          throw std::invalid_argument("Instruction working-path import required");
        skipProject=services.sync.pathInWorkingPath(currentDir,*canonicalRoot)&&
          !services.sync.pathInWorkingPath(currentDir,*gitRoot);
      }
      if(!services.sync.settingSourceEnabled)
        throw std::invalid_argument("Setting-source import required");
      if(services.sync.settingSourceEnabled("projectSettings")&&!skipProject){
        callFile(join(currentDir,"BATBOX.md"),"Project",includeExternal,
          [self=shared_from_this()]{self->projectDot();});
        return;
      }
      if(services.sync.settingSourceEnabled("localSettings")){
        callFile(join(currentDir,"BATBOX.local.md"),"Local",includeExternal,
          [self=shared_from_this()]{self->nextDir();});
        return;
      }
    }
    startAdditional();
  }catch(...){fail(std::current_exception());}
}
void MemoryRun::projectDot(){
  callFile(dotFile(currentDir),"Project",includeExternal,
    [self=shared_from_this()]{self->projectRules();});
}
void MemoryRun::projectRules(){
  callRules(rulesDir(currentDir),"Project",includeExternal,
    [self=shared_from_this()]{self->localFile();});
}
void MemoryRun::localFile(){
  try{
    if(!services.sync.settingSourceEnabled)
      throw std::invalid_argument("Setting-source import required");
    if(services.sync.settingSourceEnabled("localSettings")){
      callFile(join(currentDir,"BATBOX.local.md"),"Local",includeExternal,
        [self=shared_from_this()]{self->nextDir();});
    }else nextDir();
  }catch(...){fail(std::current_exception());}
}
void MemoryRun::startAdditional(){
  try{
    if(!services.sync.additionalDirectoriesEnabled)
      throw std::invalid_argument("Additional directory feature import required");
    if(!services.sync.additionalDirectoriesEnabled()){autoMemory();return;}
    if(!services.sync.additionalDirectories)
      throw std::invalid_argument("Additional directories import required");
    additionalDirs=services.sync.additionalDirectories();
    nextAdditional();
  }catch(...){fail(std::current_exception());}
}
void MemoryRun::nextAdditional(){
  try{
    if(additionalIndex>=additionalDirs.size()){autoMemory();return;}
    currentDir=additionalDirs[additionalIndex++];
    callFile(join(currentDir,"BATBOX.md"),"Project",includeExternal,
      [self=shared_from_this()]{self->additionalDot();});
  }catch(...){fail(std::current_exception());}
}
void MemoryRun::additionalDot(){
  callFile(dotFile(currentDir),"Project",includeExternal,
    [self=shared_from_this()]{self->additionalRules();});
}
void MemoryRun::additionalRules(){
  callRules(rulesDir(currentDir),"Project",includeExternal,
    [self=shared_from_this()]{self->nextAdditional();});
}
} // namespace batbox::instructions::project::async
