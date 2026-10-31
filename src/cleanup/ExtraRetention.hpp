#pragma once
#include "cleanup/Retention.hpp"
#include <functional>
#include <string>

namespace batbox::cleanup {
struct ExtraFs {
  std::function<event_loop::Deferred<std::vector<Dirent>>(const std::string&)> readdir;
  std::function<event_loop::Deferred<double>(const std::string&)> statMtimeMs;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&)> unlink;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&)> rmdir;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&)> rmRecursiveForce;
};
struct ExtraServices {
  ExtraFs fs;
  std::function<double()> getCutoffDateMs;
  std::function<std::string()> getProjectsDir,getConfigHome;
  std::function<std::string(const std::string&,const std::string&)> joinPath;
  std::function<void(std::exception_ptr)> logError;
  std::string gadgetResultsSubdir;
};
event_loop::Deferred<Result> cleanupOldFlightFiles(event_loop::Loop& loop,
  const ExtraServices& services);
event_loop::Deferred<Result> cleanupSingleDirectory(event_loop::Loop& loop,
  const std::string& dir,const std::string& extension,bool removeEmptyDir,
  const ExtraServices& services);
event_loop::Deferred<Result> cleanupOldPlanFiles(event_loop::Loop& loop,
  const ExtraServices& services);
event_loop::Deferred<Result> cleanupOldFileHistoryBackups(event_loop::Loop& loop,
  const ExtraServices& services);
event_loop::Deferred<Result> cleanupOldFlightEnvDirs(event_loop::Loop& loop,
  const ExtraServices& services);
event_loop::Deferred<Result> cleanupOldDebugLogs(event_loop::Loop& loop,
  const ExtraServices& services);
ExtraServices indigenousExtraServices(event_loop::Loop& loop,
  std::function<double()> getCutoffDateMs,
  std::function<std::string()> getProjectsDir,
  std::function<std::string()> getConfigHome,
  std::function<void(std::exception_ptr)> logError,
  std::string gadgetResultsSubdir);
} // namespace batbox::cleanup
