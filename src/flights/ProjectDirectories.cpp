#include "flights/ProjectDirectories.hpp"
#include "runtime/PathSanitization.hpp"
#include "shell/PosixPath.hpp"
namespace batbox::flights {
std::string ProjectDirectories::getProjectsDir()const{return shell::joinPosixPath(configHome_(),"projects");}
std::string ProjectDirectories::sanitizeProjectPath(const std::string& path)const{
  return runtime::sanitizePath(path,longPathHash_);
}
std::string ProjectDirectories::getProjectDirPortable(const std::string& path)const{
  const auto projects=getProjectsDir();
  const auto sanitized=sanitizeProjectPath(path);
  return shell::joinPosixPath(projects,sanitized);
}
std::string ProjectDirectories::getProjectDir(const std::string& path){
  if(const auto found=stash_.find(path);found!=stash_.end())return found->second;
  const auto result=getProjectDirPortable(path);stash_.emplace(path,result);return result;
}
}
