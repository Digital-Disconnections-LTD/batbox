#pragma once
#include "instructions/project/async/AsyncDiscovery.hpp"

namespace batbox::instructions::project::async {
struct MemoryRun:std::enable_shared_from_this<MemoryRun> {
  event_loop::Loop& loop;
  std::shared_ptr<HookState> hooks;
  Services services;
  bool forceIncludeExternal=false,includeExternal=false,nestedWorktree=false;
  Deferred<MemoryFiles> output;
  MemoryFiles result;
  std::shared_ptr<PathSet> processed=std::make_shared<PathSet>();
  std::string originalCwd;
  std::optional<std::string> gitRoot,canonicalRoot;
  std::vector<std::string> dirs,additionalDirs;
  std::size_t dirIndex=0,additionalIndex=0;
  std::string currentDir;
  MemoryRun(event_loop::Loop& owner,std::shared_ptr<HookState> hookState,
    Services imports,bool force):loop(owner),hooks(std::move(hookState)),
    services(std::move(imports)),forceIncludeExternal(force),output(owner){}
  void fail(std::exception_ptr error){output.reject(error);}
  void callFile(const std::string& path,const std::string& type,
    bool external,std::function<void()> next);
  void callRules(const std::string& path,const std::string& type,
    bool external,std::function<void()> next);
  void start();
  void managedRules();
  void userFile();
  void userRules();
  void setupDirs();
  void nextDir();
  void projectDot();
  void projectRules();
  void localFile();
  void startAdditional();
  void nextAdditional();
  void additionalDot();
  void additionalRules();
  void autoMemory();
  void teamMemory();
  void finish();
};
} // namespace batbox::instructions::project::async
