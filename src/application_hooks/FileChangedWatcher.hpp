#pragma once
#include "event_loop/Loop.hpp"
#include "instructions/hooks/runtime/EnvHooks.hpp"
#include "settings/watcher/IndigenousWatcher.hpp"
#include <memory>
#include <string>
#include <vector>

namespace batbox::application_hooks {
namespace hooks=instructions::hooks::runtime;
struct FileChangedInputs {
  explicit FileChangedInputs(event_loop::Loop& loop):owner(loop){}
  event_loop::Loop& owner;
  std::string cwd;
  // Read the current settings snapshot each time the watch list is rebuilt.
  std::function<Json()> hooksConfig;
  std::function<event_loop::Deferred<hooks::EnvHookResult>(
    const std::string&,const std::string&)> fileChanged,cwdChanged;
  std::function<event_loop::Deferred<event_loop::Unit>()> clearCwdEnvFiles;
  std::function<void(const std::string&,bool)> notify;
  std::function<void(const std::string&)> log;
};

class FileChangedWatcher final:
    public std::enable_shared_from_this<FileChangedWatcher> {
public:
  static std::shared_ptr<FileChangedWatcher> create(FileChangedInputs inputs);
  explicit FileChangedWatcher(FileChangedInputs inputs);
  ~FileChangedWatcher();
  void start();
  void updateWatchPaths(std::vector<std::string> paths);
  event_loop::Deferred<event_loop::Unit> onCwdChanged(
    const std::string& oldCwd,const std::string& newCwd);
  void close();
private:
  std::vector<std::string> resolveWatchPaths()const;
  void restart();
  void fileEvent(const std::string& path,const std::string& event);
  void report(const hooks::EnvHookResult& result);
  FileChangedInputs inputs_;
  std::vector<std::string> dynamicWatchPaths_,dynamicWatchPathsSorted_;
  std::unique_ptr<settings::watcher::IndigenousWatcher> watcher_;
  bool initialized_=false,closed_=false;
};
} // namespace batbox::application_hooks
