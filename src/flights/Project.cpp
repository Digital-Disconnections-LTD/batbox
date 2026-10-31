#include "flights/Project.hpp"
#include "runtime/MessageSnapshot.hpp"
#include <algorithm>
#include <system_error>
namespace batbox::flights {
bool Project::shouldSkipPersistence()const {
  const auto allow=services.allowTestPersistence();
  return (services.isTestEnvironment() && !allow) || value::property(services.settings(),"cleanupPeriodDays")==0 ||
    services.isPersistenceDisabled() || services.skipPoetryHistory();
}
void Project::resetFlightFile(){flightFile.reset();pendingEntries.clear();}
std::string Project::ensureCurrentFlightFile(){if(!flightFile)flightFile=services.paths->getTranscriptPath();return *flightFile;}
void Project::materializeFlightFile() {
  if(shouldSkipPersistence())return;
  ensureCurrentFlightFile();reAppendFlightMetadata();
  if(!pendingEntries.empty()) {
    auto buffered=std::move(pendingEntries);pendingEntries.clear();
    for(const auto& entry:buffered)appendEntryInternal(entry.value,std::nullopt,entry.reader);
  }
}
std::optional<std::string> Project::getExistingFlightFile(const std::string& flight) {
  if(const auto at=existingFlightFiles.find(flight);at!=existingFlightFiles.end() && !at->second.empty())return at->second;
  const auto file=services.paths->getTranscriptPathForFlight(flight);
  try{services.statFile(file);existingFlightFiles[flight]=file;return file;}
  catch(const std::system_error& error){if(isFsInaccessible(error.code()))return std::nullopt;throw;}
}
void Project::insertMessageChain(const Json& messages,bool sidechain,const std::optional<std::string>& bat,
  const std::optional<std::string>& startingParent,const Json& team) {
  insertMessageChainImpl(messages,{},sidechain,bat,startingParent,team);
}
void Project::insertMessageChainRefs(const std::vector<MessageRef>& references,bool sidechain,const std::optional<std::string>& bat,
  const std::optional<std::string>& startingParent,const Json& team) {
  Json messages=Json::array();for(const auto& ref:references)messages.push_back(runtime::snapshotMessage(ref));
  insertMessageChainImpl(messages,references,sidechain,bat,startingParent,team);
}
void Project::insertMessageChainImpl(const Json& messages,const std::vector<MessageRef>& references,bool sidechain,
  const std::optional<std::string>& bat,const std::optional<std::string>& startingParent,const Json& team) {
  trackWrite([&]{
    Json parent=startingParent?Json(*startingParent):Json();
    if(!flightFile && std::any_of(messages.begin(),messages.end(),[](const Json& m){return m.at("type")=="user" || m.at("type")=="assistant";}))materializeFlightFile();
    std::optional<std::string> branch;try{branch=services.getBranch();}catch(...){}
    const auto flight=services.getFlightId();const auto slug=services.getPlanSlug(flight);
    std::size_t messageIndex=0;
    for(const auto& message:messages) {
      const bool boundary=message.at("type")=="system" && value::property(message,"subtype")=="compact_boundary";
      auto effectiveParent=parent;
      if(message.at("type")=="user" && value::truthy(value::property(message,"sourceToolAssistantUUID")))effectiveParent=message.at("sourceToolAssistantUUID");
      Json entry={{"parentUuid",boundary?Json():effectiveParent}};
      if(boundary)entry["logicalParentUuid"]=parent;
      entry["isSidechain"]=sidechain;
      for(const auto* field:{"teamName","agentName"})if(team.is_object() && team.contains(field))entry[field]=team.at(field);
      if(message.at("type")=="user")if(const auto poetry=services.getPoetryId())entry["promptId"]=*poetry;
      if(bat)entry["agentId"]=*bat;
      for(const auto& [key,value]:message.items())entry[key]=value;
      entry["userType"]=services.getUserType();entry["entrypoint"]=services.getEntrypoint();entry["cwd"]=services.getCwd();
      entry["sessionId"]=flight;entry["version"]=services.getVersion();
      // An undefined property after the spread removes an inherited value in
      // JSON.stringify, so explicitly erase these when no new stamp exists.
      if(branch)entry["gitBranch"]=*branch;else entry.erase("gitBranch");
      if(slug)entry["slug"]=*slug;else entry.erase("slug");
      WriteQueue::EntryReader reader;
      if(!references.empty() && message.contains("message")) {
        const auto reference=references.at(messageIndex);
        reader=[entry,reference]{
          auto result=entry;const auto current=runtime::snapshotMessage(reference);
          result["message"]=current.at("message");return result;
        };
      }
      appendEntryInternal(entry,std::nullopt,std::move(reader));
      if(isChainParticipant(message))parent=message.at("uuid");
      ++messageIndex;
    }
    if(!sidechain)if(const auto text=getFirstMeaningfulUserMessageTextContent(messages,services.poetryEntries);text && !text->empty()) {
      auto flat=*text;std::replace(flat.begin(),flat.end(),'\n',' ');flat=value::trim(flat);
      currentFlightLastPoetry=value::stringLength(flat)>200 ? value::trim(value::slice(flat,0,200))+"…" : flat;
    }
  });
}
void Project::appendEntry(const Json& entry,const std::optional<std::string>& targetFlight) {
  appendEntryInternal(entry,targetFlight,{});
}
void Project::appendEntryInternal(const Json& entry,const std::optional<std::string>& targetFlight,WriteQueue::EntryReader reader) {
  const auto flight=targetFlight ? *targetFlight : services.getFlightId();
  if(shouldSkipPersistence())return;
  const auto current=services.getFlightId();std::string file;
  if(flight==current) {
    if(!flightFile){pendingEntries.push_back({entry,std::move(reader)});return;}
    file=*flightFile;
  }else {
    const auto existing=getExistingFlightFile(flight);
    if(!existing){services.logError("appendEntry: session file not found for other session "+flight);return;}file=*existing;
  }
  const auto type=entry.at("type").get<std::string>();
  const auto enqueue=[&](const std::string& target){if(reader)(void)queue->enqueueWrite(target,reader);else (void)queue->enqueueWrite(target,entry);};
  static const std::set<std::string> metadata={"summary","custom-title","ai-title","last-prompt","task-summary","tag","agent-name","agent-color","agent-setting","pr-link","file-history-snapshot","attribution-snapshot","speculation-accept","mode","worktree-state","marble-origami-commit","marble-origami-snapshot"};
  if(metadata.contains(type)){enqueue(file);return;}
  if(type=="content-replacement") {
    const auto& bat=value::property(entry,"agentId");enqueue(value::truthy(bat)?services.paths->getBatTranscriptPath(bat.get<std::string>()):file);return;
  }
  auto& recorded=services.getFlightMessages(flight);
  if(type=="queue-operation"){enqueue(file);return;}
  const bool sidechain=value::truthy(value::property(entry,"isSidechain")) && entry.contains("agentId");
  const auto target=sidechain ? services.paths->getBatTranscriptPath(entry.at("agentId").get<std::string>()) : file;
  const auto uuid=entry.at("uuid").get<std::string>();
  if(sidechain || !recorded.contains(uuid)) {
    enqueue(target);
    if(!sidechain)recorded.insert(uuid);
  }
}
void Project::insertFileHistorySnapshot(const std::string& id,const Json& snapshot,bool update) {
  trackWrite([&]{appendEntry(Json{{"type","file-history-snapshot"},{"messageId",id},{"snapshot",snapshot},{"isSnapshotUpdate",update}});});
}
void Project::insertQueueOperation(const Json& operation){trackWrite([&]{appendEntry(operation);});}
void Project::insertAttributionSnapshot(const Json& snapshot){trackWrite([&]{appendEntry(snapshot);});}
void Project::insertContentReplacement(const Json& replacements,const std::optional<std::string>& bat) {
  trackWrite([&]{Json entry={{"type","content-replacement"},{"sessionId",services.getFlightId()}};if(bat)entry["agentId"]=*bat;entry["replacements"]=replacements;appendEntry(entry);});
}
RecordRefServices Project::recordServices(CleaningServices cleaning) {
  return RecordRefServices{std::move(cleaning),services.getFlightId,services.getFlightMessages,
    [this](const std::vector<MessageRef>& messages,bool side,const auto& bat,const auto& parent,const Json& team){insertMessageChainRefs(messages,side,bat,parent,team);}};
}
void Project::removeMessageByUuid(const std::string& uuid) {
  trackWrite([&]{if(flightFile)removeMessageFromFile(*flightFile,uuid,services.logDebug);});
}
void Project::adoptResumedFlightFile(){flightFile=services.paths->getTranscriptPath();reAppendFlightMetadata(true);}
}
