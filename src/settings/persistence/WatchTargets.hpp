#pragma once
#include <optional>
#include <string>
#include <vector>

namespace batbox::settings::persistence {
struct WatchTargets {
  std::vector<std::string> dirs;
  std::vector<std::string> settingsFiles;
  // FileChanged hooks may explicitly watch a path inside .git. Settings watchers
  // exclude .git.
  bool includeGitPaths=false;
};

// stat follows symlinks.
struct WatchStat {
  bool file=false;
  bool directory=false;
};
bool ignoreWatchPath(const WatchTargets& targets,const std::string& path,
  const std::optional<WatchStat>& stat);
}
