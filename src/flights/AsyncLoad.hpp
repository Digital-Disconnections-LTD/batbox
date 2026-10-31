#pragma once
#include "event_loop/Deferred.hpp"
#include "flights/Load.hpp"
namespace batbox::flights {
struct AsyncLoadServices {
  bool disablePrecompactSkip=false,pruneLeaves=false;
  std::size_t skipThreshold=5*1024*1024;
  std::function<event_loop::Deferred<std::uintmax_t>(const std::string&)> stat;
  std::function<event_loop::Deferred<std::string>(const std::string&)> readFile;
  std::function<event_loop::Deferred<ScanResult>(const std::string&,std::size_t)> readTranscriptForLoad;
  std::function<event_loop::Deferred<std::vector<std::string>>(const std::string&,std::size_t)> scanPreBoundaryMetadata;
  std::function<std::string(const std::string&)> walkChainBeforeParse;
};
event_loop::Deferred<LoadedTranscript> loadTranscriptFileAsync(event_loop::Loop&,
  const std::string& path,bool keepAllLeaves,const AsyncLoadServices&);
void bindIndigenousAsyncFileReads(event_loop::Loop&,AsyncLoadServices&);
event_loop::Deferred<std::optional<Json>> getBatTranscriptAsync(event_loop::Loop&,
  const std::string& bat,Paths&,const AsyncLoadServices&,GraphServices);
} // namespace batbox::flights
