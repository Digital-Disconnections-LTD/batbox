#pragma once
#include "filesystem/FileOps.hpp"
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace batbox::fs_operations {
using Stats=filesystem::FsStat;
using FileView=filesystem::FsView;
struct PathImports {
  std::function<std::string(const std::string&)> dirname,basename;
  std::function<std::string(const std::vector<std::string>&)> join;
  std::function<bool(const std::string&)> isAbsolute;
  std::function<std::string(const std::string&,const std::string&)> resolve;
  std::function<std::string()> homedir;
  std::function<std::string(const std::string&)> normalizeNFC;
};
using ResolvedPath=filesystem::ResolvedPath;

bool isDuplicatePath(const FileView& fs,const std::string& filePath,
  std::unordered_set<std::string>& loadedPaths);
std::optional<std::string> resolveDeepestExistingAncestorSync(
  const FileView& fs,const std::string& absolutePath,const PathImports& path);
std::vector<std::string> getPathsForPermissionCheck(
  const std::string& inputPath,const FileView& fs,const PathImports& path);
}
