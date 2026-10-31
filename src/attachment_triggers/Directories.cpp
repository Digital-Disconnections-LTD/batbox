#include "attachment_triggers/Attachments.hpp"
#include <algorithm>
#include <filesystem>

namespace batbox::attachment_triggers {
namespace {
namespace fs=std::filesystem;
std::string parent(const std::string& path){
  return fs::path(path).parent_path().generic_string();
}
std::string root(const std::string& path){
  return fs::path(path).root_path().generic_string();
}
}
DirectoriesToProcess getDirectoriesToProcess(const std::string& targetPath,
  const std::string& originalCwd,const std::string& cwd){
  const auto target=fs::path(targetPath).is_absolute()?fs::path(targetPath):
    fs::path(cwd)/targetPath;
  const auto targetDir=parent(target.lexically_normal().generic_string());
  DirectoriesToProcess result;
  auto current=targetDir;
  while(current!=originalCwd&&current!=root(current)){
    if(current.starts_with(originalCwd))result.nestedDirs.push_back(current);
    current=parent(current);
  }
  std::reverse(result.nestedDirs.begin(),result.nestedDirs.end());
  current=originalCwd;
  while(current!=root(current)){
    result.cwdLevelDirs.push_back(current);
    current=parent(current);
  }
  std::reverse(result.cwdLevelDirs.begin(),result.cwdLevelDirs.end());
  return result;
}
} // namespace batbox::attachment_triggers
