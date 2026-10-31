#pragma once
#include "event_loop/Deferred.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::cleanup {
struct StashEntry {std::string key;double timeMs=0;};
struct StashEntryStream {
  std::function<event_loop::Deferred<std::optional<StashEntry>>()> next;
};
struct StashApi {
  std::function<StashEntryStream(const std::string&)> listStream;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&,
    const std::string&)> removeEntry;
};
struct StashThrottleServices {
  std::function<std::string()> configHome,homeDir,nowIso;
  std::function<double()> nowMs;
  std::function<event_loop::Deferred<double>(const std::string&)> statMtimeMs;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&,
    const std::string&)> writeFile;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&)> lock,
    unlock;
  std::function<event_loop::Deferred<StashApi>()> loadStashApi;
  std::function<event_loop::Deferred<event_loop::Unit>()> cleanupOldVersions;
  std::function<void(const std::string&)> debug;
  std::function<void(std::exception_ptr)> logError;
  std::function<std::string(const std::string&,const std::string&)> joinPath;
  std::string packageKeySubstring,packageDebugLabel;
};
event_loop::Deferred<event_loop::Unit> cleanupPackageStashThrottled(
  event_loop::Loop& loop,const StashThrottleServices& services);
event_loop::Deferred<event_loop::Unit> cleanupOldVersionsThrottled(
  event_loop::Loop& loop,const StashThrottleServices& services);
} // namespace batbox::cleanup
