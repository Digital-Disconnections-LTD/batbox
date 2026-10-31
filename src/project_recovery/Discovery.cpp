#include "project_recovery/Internal.hpp"
#include <system_error>

namespace batbox::project_recovery {
namespace fs=std::filesystem;
namespace detail {
std::optional<fs::path> startDirectory(const fs::path& cwd){
  std::error_code error;
  const auto absolute=fs::absolute(cwd,error);
  if(error)return std::nullopt;
  auto path=fs::weakly_canonical(absolute,error);
  if(error||!fs::is_directory(path,error)||error)return std::nullopt;
  return path;
}
bool plainDirectory(const fs::path& path){
  std::error_code error;
  return fs::is_directory(fs::symlink_status(path,error))&&!error;
}
bool plainFile(const fs::path& path){
  std::error_code error;
  return fs::is_regular_file(fs::symlink_status(path,error))&&!error;
}
static bool gitBoundary(const fs::path& path){
  std::error_code error;
  return fs::exists(fs::symlink_status(path/".git",error))&&!error;
}
fs::path projectRoot(const fs::path& cwd){
  const auto start=startDirectory(cwd);
  if(!start)return {};
  auto current=*start;
  for(;;){
    if(gitBoundary(current))return current;
    const auto parent=current.parent_path();
    if(parent==current)return *start;
    current=parent;
  }
}
}
std::optional<fs::path> findDirectory(const fs::path& cwd){
  const auto start=detail::startDirectory(cwd);
  if(!start)return std::nullopt;
  auto current=*start;
  for(;;){
    const auto directory=current/".batbox";
    if(detail::plainDirectory(directory)&&
      (detail::plainFile(directory/"Makefile")||
       detail::plainFile(directory/"00_project.md")||
       detail::plainFile(directory/"10_memory.md")))
      return directory;
    if(detail::gitBoundary(current))return std::nullopt;
    const auto parent=current.parent_path();
    if(parent==current)return std::nullopt;
    current=parent;
  }
}
}
