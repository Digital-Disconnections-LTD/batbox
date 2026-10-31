#pragma once
#include "event_loop/Deferred.hpp"
#include "instructions/project/ProjectDiscovery.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::instructions::project::async {
using event_loop::Deferred;
using MemoryFiles=std::vector<MemoryFileInfo>;
struct Dirent {
  std::string name;
  bool isDirectory=false,isFile=false;
};
struct FileKind {bool isDirectory=false,isFile=false;};
struct Services {
  DiscoveryServices sync;
  std::function<Deferred<std::string>(const std::string&)> readFile;
  std::function<Deferred<std::vector<Dirent>>(const std::string&)> readdir;
  std::function<Deferred<FileKind>(const std::string&)> stat;
};
struct HookState {
  std::string nextEagerLoadReason="session_start";
  bool shouldFireHook=true;
};
Services indigenousAsyncFs(event_loop::Loop& loop,DiscoveryServices sync);
Deferred<ParsedMemoryContent> safelyReadMemoryFile(event_loop::Loop& loop,
  const std::string& path,const std::string& type,
  const std::optional<std::string>& includeBasePath,const Services& services);
Deferred<MemoryFiles> processMemoryFile(event_loop::Loop& loop,
  const std::string& path,const std::string& type,
  std::shared_ptr<PathSet> processedPaths,bool includeExternal,
  const Services& services,int depth=0,
  const std::optional<std::string>& parent={});
Deferred<MemoryFiles> processMdRules(event_loop::Loop& loop,
  const std::string& rulesDir,const std::string& type,
  std::shared_ptr<PathSet> processedPaths,bool includeExternal,
  bool conditionalRule,const Services& services,
  std::shared_ptr<PathSet> visitedDirs={});
Deferred<MemoryFiles> processConditionedMdRules(event_loop::Loop& loop,
  const std::string& targetPath,const std::string& rulesDir,
  const std::string& type,std::shared_ptr<PathSet> processedPaths,
  bool includeExternal,const Services& services);
Deferred<MemoryFiles> getManagedAndUserConditionalRules(event_loop::Loop& loop,
  const std::string& targetPath,std::shared_ptr<PathSet> processedPaths,
  const Services& services);
Deferred<MemoryFiles> getMemoryFilesForNestedDirectory(event_loop::Loop& loop,
  const std::string& dir,const std::string& targetPath,
  std::shared_ptr<PathSet> processedPaths,const Services& services);
Deferred<MemoryFiles> getConditionalRulesForCwdLevelDirectory(
  event_loop::Loop& loop,const std::string& dir,const std::string& targetPath,
  std::shared_ptr<PathSet> processedPaths,const Services& services);
class MemoryDiscoveryStash {
public:
  explicit MemoryDiscoveryStash(event_loop::Loop& loop):loop_(loop){}
  Deferred<MemoryFiles> getMemoryFiles(const Services& services);
  Deferred<MemoryFiles> getMemoryFiles(bool forceIncludeExternal,const Services& services);
  void clearMemoryFileStashes();
  void resetGetMemoryFilesStash(const std::string& reason="session_start");
private:
  event_loop::Loop& loop_;
  std::optional<Deferred<MemoryFiles>> normal_,explicitFalse_,forced_;
  std::shared_ptr<HookState> hooks_=std::make_shared<HookState>();
  Deferred<MemoryFiles> load(std::optional<bool> forceArgument,const Services& services);
};
void finishInstructionsLoadedHooks(const std::shared_ptr<HookState>& hooks,
  const MemoryFiles& files,bool forceIncludeExternal,const Services& services);
} // namespace batbox::instructions::project::async
