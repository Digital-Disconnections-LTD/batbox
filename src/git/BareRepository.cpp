#include "git/BareRepository.hpp"
#include "shell/PosixPath.hpp"
#include <stdexcept>

namespace batbox::git {
bool isCurrentDirectoryBareGitRepo(const BareRepositoryImports& imports){
  if(!imports.getCwd||!imports.statSync)
    throw std::invalid_argument("Bare Git repository imports required");
  const auto cwd=imports.getCwd();
  const auto gitPath=shell::joinPosixPath(cwd,".git");
  try{
    const auto stats=imports.statSync(gitPath);
    if(stats.isFile)return false;
    if(stats.isDirectory){
      const auto gitHeadPath=shell::joinPosixPath(gitPath,"HEAD");
      try{if(imports.statSync(gitHeadPath).isFile)return false;}catch(...){}
    }
  }catch(...){}
  try{if(imports.statSync(shell::joinPosixPath(cwd,"HEAD")).isFile)return true;}catch(...){}
  try{if(imports.statSync(shell::joinPosixPath(cwd,"objects")).isDirectory)return true;}catch(...){}
  try{if(imports.statSync(shell::joinPosixPath(cwd,"refs")).isDirectory)return true;}catch(...){}
  return false;
}
} // namespace batbox::git
