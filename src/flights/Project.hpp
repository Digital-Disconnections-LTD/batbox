#pragma once
#include "flights/Load.hpp"
#include "flights/WriteQueue.hpp"
#include "flights/Metadata.hpp"
#include "flights/Tombstone.hpp"
#include "flights/FlightState.hpp"
namespace batbox::flights {
struct ProjectServices {
  Paths* paths=nullptr;
  std::function<bool()> isTestEnvironment,allowTestPersistence,isPersistenceDisabled,skipPoetryHistory;
  std::function<Json()> settings;
  std::function<std::optional<std::string>()> getBranch,getPoetryId;
  std::function<std::string()> getFlightId,getUserType,getEntrypoint,getCwd,getVersion;
  std::function<std::optional<std::string>(const std::string&)> getPlanSlug;
  std::function<runtime::OrderedStringSet&(const std::string&)> getFlightMessages;
  std::function<void(const std::string&)> statFile,logError;
  std::function<std::string(const std::string&)> readTail=readFileTailSync;
  std::function<void(const std::string&,const Json&)> appendMetadata=appendEntryToFile;
  std::function<std::string()> nowIso;
  TombstoneDiagnostic logDebug;
  PoetryServices poetryEntries;
};
class Project:public FlightState {
  ProjectServices services;
  std::shared_ptr<WriteQueue> queue;
  template<class F> void trackWrite(F&& fn) {
    queue->incrementPendingWrites();
    try{fn();}catch(...){queue->decrementPendingWrites();throw;}
    queue->decrementPendingWrites();
  }
  void materializeFlightFile();
  std::string ensureCurrentFlightFile();
  std::optional<std::string> getExistingFlightFile(const std::string& flight);
  void appendEntryInternal(const Json& entry,const std::optional<std::string>& flight,WriteQueue::EntryReader reader);
  void insertMessageChainImpl(const Json& messages,const std::vector<MessageRef>& references,bool sidechain,
    const std::optional<std::string>& bat,const std::optional<std::string>& parent,const Json& team);
public:
  Project(ProjectServices services,std::shared_ptr<WriteQueue> queue):services(std::move(services)),queue(std::move(queue)){}
  bool shouldSkipPersistence()const;
  void resetFlightFile();
  const std::optional<std::string>& currentFile()const{return flightFile;}
  void reAppendFlightMetadata(bool skipTitleRefresh=false);
  void insertMessageChain(const Json& messages,bool sidechain=false,
    const std::optional<std::string>& bat=std::nullopt,const std::optional<std::string>& startingParent=std::nullopt,
    const Json& team=Json());
  void insertMessageChainRefs(const std::vector<MessageRef>& messages,bool sidechain=false,
    const std::optional<std::string>& bat=std::nullopt,const std::optional<std::string>& startingParent=std::nullopt,
    const Json& team=Json());
  void appendEntry(const Json& entry,const std::optional<std::string>& flight=std::nullopt);
  void insertFileHistorySnapshot(const std::string& messageId,const Json& snapshot,bool isUpdate);
  void insertQueueOperation(const Json& operation);
  void insertAttributionSnapshot(const Json& snapshot);
  void insertContentReplacement(const Json& replacements,const std::optional<std::string>& bat=std::nullopt);
  void removeMessageByUuid(const std::string& uuid);
  void adoptResumedFlightFile();
  void flush(){queue->flush();}
  RecordRefServices recordServices(CleaningServices cleaning);
};
}
