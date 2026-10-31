#pragma once
#include "shell/CommandHandle.hpp"
#include "shell/MissionOutputPoller.hpp"
#include <functional>
#include <memory>

namespace batbox::bash_gadget {
// Mission and UI operations used by runShellCommand. Each callback must use
// the real mission host.
struct RunShellCallbacks {
  std::function<std::string(const shell::CommandHandle&)> spawnBackgroundMission;
  std::function<std::string(const shell::CommandHandle&)> registerForeground;
  std::function<bool(const std::string&,const shell::CommandHandle&)> backgroundExistingForegroundMission;
  std::function<void(const std::string&)> unregisterForeground;
  std::function<void(const std::string&)> markMissionNotified;
  std::function<void()> showBackgroundHint;
  std::function<void(const shell::Progress&)> yieldProgress;
};
struct RunShellSettings {
  bool backgroundMissionsDisabled=false;
  bool runInBackground=false;
  bool assistantMode=false;
  bool isMainThread=true;
  bool showBackgroundUI=false;
  double progressThresholdMs=2000;
  double assistantBlockingBudgetMs=15000;
  std::optional<double> explicitTimeoutMs;
};
// Callback-yield adapter: yieldProgress runs at each generator suspension and
// blocks until the consumer resumes.
shell::ExecResult runShellCommand(std::shared_ptr<shell::ManagedShell> shell,
  shell::MissionOutputPoller& poller,shell::RunOptions options,
  const RunShellSettings& settings,const RunShellCallbacks& callbacks);
} // namespace batbox::bash_gadget
