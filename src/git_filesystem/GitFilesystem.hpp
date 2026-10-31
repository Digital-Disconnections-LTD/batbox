#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::git_filesystem {
using event_loop::Deferred;
using event_loop::Unit;
struct FileStat {bool isFile=false,isDirectory=false;};
struct Head {std::string type,value;}; // type "branch" or "detached"
struct Services {
  std::function<Deferred<std::string>(const std::string&)> readFile;
  std::function<Deferred<FileStat>(const std::string&)> stat;
  std::function<Deferred<std::vector<std::string>>(const std::string&)> readdir;
  std::function<std::optional<std::string>(const std::string&)> findGitRoot;
  std::function<std::string()> getCwd;
  std::function<Deferred<std::optional<std::string>>(
    const std::string&,const std::string&,const std::optional<std::string>&,
    const std::string&)> parseGitConfigValue;
  std::function<Deferred<Unit>()> waitForScrollIdle;
  std::function<void(const std::string&,std::uint64_t,std::function<void()>)> watchFile;
  std::function<void(const std::string&)> unwatchFile;
  std::function<void(std::function<void()>)> registerCleanup;
  std::uint64_t watchIntervalMs=1000;
};
bool isSafeRefName(const std::string& name);
bool isValidGitSha(const std::string& sha);
class GitFilesystem {
public:
  // Owner-loop object; keep it alive until pending reads and posted watcher
  // callbacks have settled.
  GitFilesystem(event_loop::Loop& loop,Services services);
  ~GitFilesystem();
  Deferred<std::optional<std::string>> resolveGitDir(std::optional<std::string> startPath={});
  void clearResolveGitDirStash();
  Deferred<std::optional<Head>> readGitHead(const std::string& gitDir);
  Deferred<std::optional<std::string>> resolveRef(const std::string& gitDir,const std::string& ref);
  Deferred<std::optional<std::string>> getCommonDir(const std::string& gitDir);
  Deferred<std::optional<std::string>> readRawSymref(const std::string& gitDir,
    const std::string& refPath,const std::string& branchPrefix);
  Deferred<std::string> getStashedBranch();
  Deferred<std::string> getStashedHead();
  Deferred<std::optional<std::string>> getStashedRemoteUrl();
  Deferred<std::string> getStashedDefaultBranch();
  void resetGitFileWatcher();
  Deferred<std::optional<std::string>> getHeadForDir(const std::string& cwd);
  Deferred<std::optional<std::string>> readWorktreeHeadSha(const std::string& worktreePath);
  Deferred<std::optional<std::string>> getRemoteUrlForDir(const std::string& cwd);
  Deferred<bool> isShallowClone();
  Deferred<std::size_t> getWorktreeCountFromFs();
  // Testable watcher boundary. Indigenous watchFile uses polling like Node.
  Deferred<Unit> ensureStarted();
  void invalidate();
private:
  struct Watcher;
  event_loop::Loop& loop_;
  Services services_;
  std::map<std::string,std::optional<std::string>> gitDirStash_;
  std::shared_ptr<Watcher> watcher_;
  std::shared_ptr<std::atomic_bool> alive_;
  void stopWatching();
  Deferred<std::optional<std::string>> resolveRefInDir(const std::string& dir,const std::string& ref);
  Deferred<Unit> watchCurrentBranchRef();
  void onHeadChanged();
  Deferred<std::string> computeBranch();
  Deferred<std::string> computeHead();
  Deferred<std::optional<std::string>> computeRemoteUrl();
  Deferred<std::string> computeDefaultBranch();
  Deferred<Json> getStashedValue(const std::string& key,
    std::function<Deferred<Json>()> compute);
};
// Indigenous FS reads/watches post completion to the supplied owner loop. Callers
// supply findGitRoot, parseGitConfigValue and scroll-idle imports separately.
Services indigenousGitFsServices(event_loop::Loop& loop);
} // namespace batbox::git_filesystem
