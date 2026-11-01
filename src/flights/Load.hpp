#pragma once
#include "flights/Transcript.hpp"
#include "flights/Paths.hpp"
namespace batbox::flights {
struct ScanResult {std::size_t boundaryStartOffset=0;std::string postBoundaryBuf;bool hasPreservedSegment=false;};
struct LoadServices {
  bool disablePrecompactSkip=false,pruneLeaves=false;
  std::size_t skipThreshold=5*1024*1024;
  std::function<std::uintmax_t(const std::string&)> stat;
  std::function<std::string(const std::string&)> readFile;
  std::function<ScanResult(const std::string&,std::size_t)> readTranscriptForLoad;
  std::function<std::vector<std::string>(const std::string&,std::size_t)> scanPreBoundaryMetadata;
  std::function<std::string(const std::string&)> walkChainBeforeParse;
};
struct LoadedTranscript {
  MessageMap messages;
  // String-keyed maps retain insertion order.
  Json summaries=Json::object(),customTitles=Json::object(),tags=Json::object();
  Json aiTitles=Json::object();
  Json batNames=Json::object(),batColors=Json::object(),batSettings=Json::object();
  Json prNumbers=Json::object(),prUrls=Json::object(),prRepositories=Json::object();
  Json modes=Json::object(),worktreeStates=Json::object();
  Json fileHistorySnapshots=Json::object(),attributionSnapshots=Json::object();
  Json contentReplacements=Json::object(),batContentReplacements=Json::object();
  Json contextCollapseCommits=Json::array();
  std::optional<Json> contextCollapseSnapshot;
  std::vector<std::string> leafUuids;
};
Json parseJSONLBuffer(const std::string& buffer);
LoadedTranscript loadTranscriptFile(const std::string& path,bool keepAllLeaves,LoadServices& services);
std::optional<Json> getBatTranscript(const std::string& bat,Paths& paths,LoadServices& services,GraphServices& graph);
std::optional<Json> selectBatTranscript(const LoadedTranscript&,const std::string& bat,GraphServices&);
// Bind filesystem reads; the three large-file algorithms remain separate services.
void bindIndigenousFileReads(LoadServices& services);
}
