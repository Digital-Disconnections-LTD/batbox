#pragma once
#include "flights/FlightState.hpp"
#include "flights/async_queue/Queue.hpp"
#include "flights/Transcript.hpp"
#include <set>
#include <system_error>

namespace batbox::flights::async_project {
using Completion=async_queue::Completion;
using event_loop::Deferred;
using event_loop::Unit;
struct Imports {
  std::function<bool()> shouldSkipPersistence;
  std::function<std::string()> getFlightId,getUserType,getEntrypoint,getCwd,getVersion;
  std::function<std::optional<std::string>()> getPoetryId;
  std::function<Deferred<std::optional<std::string>>()> getBranch;
  std::function<std::optional<std::string>(const std::string&)> getPlanSlug;
  std::function<Deferred<std::shared_ptr<runtime::OrderedStringSet>>(const std::string&)> getFlightMessages;
  std::function<Completion(const std::string&)> statFile;
  std::function<bool(std::exception_ptr)> isFsInaccessible;
  std::function<std::string()> getTranscriptPath;
  std::function<std::string(const std::string&)> getTranscriptPathForFlight,getBatTranscriptPath;
  std::function<void(const std::string&)> logError;
  // Reuse the metadata stash and tail lookup. This synchronous method writes
  // its own metadata entries.
  std::function<void(FlightState&,bool skipTitleRefresh)> reAppendFlightMetadata;
  PoetryServices poetryEntries;
  CleaningServices cleaning;
};

class AsyncProject {
public:
  AsyncProject(event_loop::Loop& loop,std::shared_ptr<async_queue::Queue> queue,
    std::shared_ptr<FlightState> state,Imports imports);
  Completion appendEntry(Json entry,std::optional<std::string> flight=std::nullopt,
    async_queue::EntryReader reader={});
  Completion materializeFlightFile();
  Deferred<std::optional<std::string>> getExistingFlightFile(const std::string& flight);
  Completion insertMessageChainRefs(std::vector<MessageRef> messages,bool sidechain=false,
    std::optional<std::string> bat=std::nullopt,
    std::optional<std::string> startingParent=std::nullopt,Json team=Json::object());
  Completion insertMessageChain(Json messages,bool sidechain=false,
    std::optional<std::string> bat=std::nullopt,
    std::optional<std::string> startingParent=std::nullopt,Json team=Json::object());
  Deferred<std::optional<std::string>> recordTranscriptRefs(
    const std::vector<MessageRef>& messages,const Json& team,
    std::optional<std::string> startingParent,const std::vector<MessageRef>& all);
  Completion recordSidechainTranscriptRefs(const std::vector<MessageRef>& messages,
    std::optional<std::string> bat,std::optional<std::string> startingParent);
  Completion insertFileHistorySnapshot(std::string messageId,Json snapshot,bool update);
  Completion insertQueueOperation(Json operation);
  Completion insertAttributionSnapshot(Json snapshot);
  Completion insertContentReplacement(Json replacements,std::optional<std::string> bat);
  Completion recordContextCollapseCommit(Json commit);
  Completion recordContextCollapseSnapshot(Json snapshot);
  Completion flush();
  void resetFlightFile();
  void adoptResumedFlightFile();
  FlightState& state(){return *state_;}
  const FlightState& state()const{return *state_;}
  event_loop::Loop& loop(){return loop_;}
  const Imports& imports()const{return imports_;}
  std::shared_ptr<async_queue::Queue> queue(){return queue_;}
private:
  event_loop::Loop& loop_;
  std::shared_ptr<async_queue::Queue> queue_;
  std::shared_ptr<FlightState> state_;
  Imports imports_;
  std::string ensureCurrentFlightFile();
};

// A real asynchronous stat service for getExistingFlightFile.
std::function<Completion(const std::string&)> indigenousStatFile(event_loop::Loop& loop);
} // namespace batbox::flights::async_project
