#pragma once
#include "filesystem/FileOps.hpp"
#include "gadgets/Gadget.hpp"
#include <functional>
#include <string>
#include <vector>

namespace batbox::gadgets::files {
struct GlobBase {std::string baseDir;std::string relativePattern;};
struct GlobResult {std::vector<std::string> files;bool truncated=false;};
struct GlobServices {
  filesystem::PathServices paths;
  std::function<std::optional<std::string>(const std::string&)> suggestPathUnderCwd;
  std::function<std::vector<std::string>(const Json&)> getFileReadIgnorePatterns;
  std::function<std::vector<std::string>(const std::vector<std::string>&,const std::string&)> normalizePatternsToPath;
  std::function<std::vector<std::string>(const std::string&)> getGlobExclusionsForPluginStash;
  std::function<std::vector<std::string>(const std::vector<std::string>&,const std::string&,std::stop_token)> ripGrep;
  std::function<double()> nowMs;
  std::function<bool()> noIgnore;
  std::function<bool()> hidden;
  std::function<std::string()> platform;
  std::string fileNotFoundCwdNote="Note: your current working directory is";
};
GlobBase extractGlobBaseDirectory(const std::string& pattern,const GlobServices& services);
GlobResult glob(const std::string& filePattern,const std::string& cwd,
                std::size_t limit,std::size_t offset,const Json& gadgetPermissionContext,
                std::stop_token stopToken,const GlobServices& services);
ValidationResult validateGlobInput(const Json& input,const GlobServices& services);
GadgetResult callGlob(const Json& input,const GadgetCallContext& context,const GlobServices& services);
Json mapGlobResult(const Json& output,const std::string& gadgetUseID);
std::string toRelativePath(const std::string& absolutePath,const std::string& cwd);
} // namespace batbox::gadgets::files
