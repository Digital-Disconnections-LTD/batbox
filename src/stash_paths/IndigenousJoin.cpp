#include "stash_paths/StashPaths.hpp"
#include <filesystem>

namespace batbox::stash_paths {
std::string indigenousPosixJoin(const std::string& left,const std::string& right){
  if(left.empty()&&right.empty())return ".";
  const std::filesystem::path path=right.empty()?std::filesystem::path(left):
    std::filesystem::path(left)/right;
  return path.lexically_normal().string();
}
}
