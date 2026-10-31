#pragma once
#include "settings/Paths.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::settings::persistence {
struct WatchTargets {
  std::vector<std::string> dirs;
  std::vector<std::string> settingsFiles;
  std::optional<std::string> dropInDir;
  // FileChanged hooks may explicitly watch a path inside .git. Settings watchers
  // exclude .git.
  bool includeGitPaths=false;
};

// stat follows symlinks.
struct WatchStat {
  bool file=false;
  bool directory=false;
};
using WatchStatReader=std::function<std::optional<WatchStat>(
  const std::string&)>;
struct WatchPathServices {
  std::function<std::optional<std::string>(Source)> getPathForSource;
  std::function<std::string()> getManagedDropInDir;
};
WatchPathServices bindWatchPaths(Paths& paths);
WatchStatReader indigenousWatchStatReader();
WatchTargets getWatchTargets(const WatchPathServices& paths,
  const WatchStatReader& stat);
std::optional<Source> getSourceForPath(const WatchPathServices& paths,
  const std::string& path);
std::string settingSourceToConfigChangeSource(Source source);
bool ignoreWatchPath(const WatchTargets& targets,const std::string& path,
  const std::optional<WatchStat>& stat);
}
