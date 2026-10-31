#pragma once
#include "event_loop/Deferred.hpp"
#include "file_state_stash/FileStateStash.hpp"
#include "instructions/MemoryPath.hpp"
#include "instructions/MemoryScan.hpp"
#include "query/GadgetTurn.hpp"
#include <stop_token>
#include <unordered_set>

namespace batbox::memory_prefetch {
struct RelevantMemory {std::string path;double mtimeMs=0;};
struct Services {
  event_loop::Loop& loop;
  instructions::MemoryPathServices paths;
  instructions::MemoryPathStash* pathStash=nullptr;
  std::string cwd;
  std::function<bool()> searchFeatureEnabled;
  std::function<std::string()> defaultStandardModel;
  std::function<event_loop::Deferred<Json>(const Json& request,
    std::stop_token signal)> sideQuery;
  std::function<void(const std::string&)> logWarning;
  std::function<void(std::exception_ptr)> logError;
  std::function<bool(const std::exception&)> isAbortError;
  std::function<double()> nowMillis;
  // Indigenous lifetime accounting for retaining a roost while its nonblocking
  // prefetch is still scanning, selecting, reading or canceling.
  std::function<void(std::stop_source)> prefetchStarted,prefetchFinished;
};

std::string memoryHeader(const std::string& path,double mtimeMs,
  double nowMs);
event_loop::Deferred<std::vector<std::string>> selectRelevantMemories(
  const std::string& query,const std::vector<instructions::MemoryHeader>& memories,
  const std::vector<std::string>& recentGadgets,std::stop_token signal,
  const Services& services);
event_loop::Deferred<std::vector<RelevantMemory>> findRelevantMemories(
  const std::string& query,const std::string& memoryDir,
  const std::vector<std::string>& recentGadgets,
  const std::unordered_set<std::string>& alreadySurfaced,
  std::stop_token signal,const Services& services);
event_loop::Deferred<Json> readMemoriesForSurfacing(
  const std::vector<RelevantMemory>& selected,std::stop_token signal,
  const Services& services);
event_loop::Deferred<Json> getRelevantMemoryAttachments(
  const std::string& input,const Json& bats,
  const std::shared_ptr<file_state_stash::FileStateStash>& readFileState,
  const std::vector<std::string>& recentGadgets,std::stop_token signal,
  const std::unordered_set<std::string>& alreadySurfaced,
  const Services& services);
query::MemoryPrefetch startRelevantMemoryPrefetch(const Json& messages,
  const execution::Context& gadgetUseContext,const Services& services);
} // namespace batbox::memory_prefetch
