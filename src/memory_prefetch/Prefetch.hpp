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
  // True when memories for a turn on this model are picked by keyword overlap
  // with their names and descriptions instead of a model request.
  std::function<bool(const std::string& model)> keywordSelection;
  // Set per prefetch from keywordSelection.
  bool selectByKeywords=false;
};

// Up to five filenames whose name or description shares words with query,
// best match first; empty when nothing matches well enough.
std::vector<std::string> selectMemoriesByKeywords(const std::string& query,
  const std::vector<instructions::MemoryHeader>& memories);
// The longest UTF-8 prefix of text within limit bytes, marked when cut.
std::string boundedQuery(const std::string& text,std::size_t limit);

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
