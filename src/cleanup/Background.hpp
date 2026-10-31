#pragma once
#include "cleanup/Retention.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::cleanup {
struct BackgroundServices {
  std::function<std::size_t()> settingsErrorCount;
  std::function<bool(const std::string&)> rawSettingsContainsKey;
  std::function<void(const std::string&)> debug;
  std::function<event_loop::Deferred<Result>()> oldMessageFiles,oldFlightFiles,
    oldPlanFiles,oldFileHistoryBackups,oldFlightEnvDirs,oldDebugLogs;
  std::function<event_loop::Deferred<event_loop::Unit>()> oldImageStashes;
  std::function<event_loop::Deferred<event_loop::Unit>(double)> oldPastes;
  std::function<event_loop::Deferred<int>(double)> staleBatWorktrees;
  std::function<event_loop::Deferred<event_loop::Unit>()> packageStashCleanup;
  std::function<double()> getCutoffDateMs;
  std::optional<std::string> userType;
  std::string internalUserType;
};
event_loop::Deferred<event_loop::Unit> cleanupOldMessageFilesInBackground(
  event_loop::Loop& loop,const BackgroundServices& services);
} // namespace batbox::cleanup
