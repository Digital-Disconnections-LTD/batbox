#pragma once
#include "instructions/project/ProjectMemory.hpp"
#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace batbox::instructions::project {
using PathSet = std::unordered_set<std::string>;
struct DiscoveryServices {
  ParseServices parse;
  std::function<std::vector<std::string>()> excludePatterns;
  // picomatch.isMatch(path, patterns, {dot:true}) is an imported service.
  std::function<bool(const std::string&,const std::vector<std::string>&)> matchExclude;
  std::function<bool(const std::string&,const std::string&)> pathInWorkingPath;
  std::function<std::string()> originalCwd;
  std::function<std::string(const std::string&)> memoryPath;
  std::function<std::string()> managedRulesDir,userRulesDir;
  std::function<bool(const std::string&)> settingSourceEnabled;
  std::function<bool()> externalIncludesApproved,additionalDirectoriesEnabled;
  std::function<std::vector<std::string>()> additionalDirectories;
  std::function<std::optional<std::string>(const std::string&)> findGitRoot,
    findCanonicalGitRoot;
  std::function<bool()> autoMemoryEnabled,teamMemoryEnabled;
  std::function<std::string()> autoMemoryEntrypoint,teamMemoryEntrypoint;
  std::function<bool()> hasInstructionsLoadedHook;
  std::function<void(const MemoryFileInfo&,const std::string&)> instructionsLoadedHook;
  // `ignore().add(globs).ignores(relativePath)` is an imported service.
  std::function<bool(const std::string&,const std::vector<std::string>&)>
    matchRuleIgnore;
  std::function<bool()> skipAutoMemoryIndex,skipProjectInstructions,
    teamMemoryFeature,externalIncludesWarningShown;
};

std::string normalizePathForComparison(const std::string& path,
  const DiscoveryServices& services);
std::vector<std::string> resolveExcludePatterns(
  const std::vector<std::string>& patterns);
bool isBatboxMdExcluded(const std::string& path,const std::string& type,
  const DiscoveryServices& services);
ParsedMemoryContent safelyReadMemoryFileAsync(const std::string& path,
  const std::string& type,const std::optional<std::string>& includeBasePath,
  const DiscoveryServices& services);
std::vector<MemoryFileInfo> processMemoryFile(const std::string& path,
  const std::string& type,PathSet& processedPaths,bool includeExternal,
  const DiscoveryServices& services,int depth=0,
  const std::optional<std::string>& parent=std::nullopt);
std::vector<MemoryFileInfo> processMdRules(const std::string& rulesDir,
  const std::string& type,PathSet& processedPaths,bool includeExternal,
  bool conditionalRule,const DiscoveryServices& services,
  PathSet* visitedDirs=nullptr);

class MemoryDiscoveryStash {
 public:
  std::vector<MemoryFileInfo> getMemoryFiles(bool forceIncludeExternal,
    const DiscoveryServices& services);
  void clearMemoryFileStashes();
  void resetGetMemoryFilesStash(const std::string& reason="session_start");
 private:
  std::optional<std::vector<MemoryFileInfo>> normal_,forced_;
  std::exception_ptr normalError_,forcedError_;
  std::string nextEagerLoadReason_="session_start";
  bool shouldFireHook_=true;
};

std::vector<MemoryFileInfo> getLargeMemoryFiles(
  const std::vector<MemoryFileInfo>& files);
std::vector<MemoryFileInfo> filterInjectedMemoryFiles(
  const std::vector<MemoryFileInfo>& files,const DiscoveryServices& services);
std::string getBatboxMds(const std::vector<MemoryFileInfo>& files,
  const DiscoveryServices& services,
  const std::function<bool(const std::string&)>& filter={});
std::vector<MemoryFileInfo> processConditionedMdRules(
  const std::string& targetPath,const std::string& rulesDir,
  const std::string& type,PathSet& processedPaths,bool includeExternal,
  const DiscoveryServices& services);
std::vector<MemoryFileInfo> getManagedAndUserConditionalRules(
  const std::string& targetPath,PathSet& processedPaths,
  const DiscoveryServices& services);
std::vector<MemoryFileInfo> getMemoryFilesForNestedDirectory(
  const std::string& dir,const std::string& targetPath,
  PathSet& processedPaths,const DiscoveryServices& services);
std::vector<MemoryFileInfo> getConditionalRulesForCwdLevelDirectory(
  const std::string& dir,const std::string& targetPath,
  PathSet& processedPaths,const DiscoveryServices& services);
struct ExternalInclude {std::string path,parent;};
std::vector<ExternalInclude> getExternalBatboxMdIncludes(
  const std::vector<MemoryFileInfo>& files,const DiscoveryServices& services);
bool hasExternalBatboxMdIncludes(const std::vector<MemoryFileInfo>& files,
  const DiscoveryServices& services);
bool shouldShowBatboxMdExternalIncludesWarning(MemoryDiscoveryStash& stash,
  const DiscoveryServices& services);
bool isMemoryFilePath(const std::string& path);
std::vector<std::string> getAllMemoryFilePaths(
  const std::vector<MemoryFileInfo>& files,
  const std::vector<std::string>& readFileStateKeys);
}
