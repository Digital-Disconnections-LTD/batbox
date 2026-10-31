#pragma once
#include <functional>
#include <string>

namespace batbox::git {
struct BareStat {bool isFile=false,isDirectory=false;};
struct BareRepositoryImports {
  std::function<std::string()> getCwd;
  std::function<BareStat(const std::string&)> statSync;
};
bool isCurrentDirectoryBareGitRepo(const BareRepositoryImports& imports);
BareRepositoryImports indigenousBareRepositoryImports(std::function<std::string()> getCwd);
} // namespace batbox::git
