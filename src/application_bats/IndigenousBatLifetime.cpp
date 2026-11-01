#include "application_bats/IndigenousBatApplication.hpp"
#include "application/Async.hpp"
#include "flights/SavedBats.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/Paths.hpp"
#include "runtime/Platform.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::application_bats {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::size_t textBytes(const Json& value){
  if(value.is_string())return value.get_ref<const std::string&>().size();
  std::size_t total=0;
  if(value.is_structured())for(const auto& item:value)total+=textBytes(item);
  return total;
}
}
void IndigenousBatApplication::releaseInactiveState(){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Agent reclamation requires owner");
  sweepSettledMissions();
  // Observation/resume can still be hydrating the deduplication set on a worker.
  if(conversationJobs_)return;
  const auto state=appState();const auto& missions=field(state,"tasks");
  std::vector<std::string> evicted;
  {
    std::lock_guard lock(conversationMutex_);
    const auto idle=[&](const std::string& id){
      const auto& mission=value::property(missions,id);
      const auto pending=pendingDirectInputs_.find(id);
      return !batJobs_.contains(id)&&!pendingResumes_.contains(id)&&
        !pendingContinuationScheduled_.contains(id)&&
        (pending==pendingDirectInputs_.end()||pending->second.empty())&&
        field(mission,"status")!="running"&&field(mission,"retain")!=true;
    };
    for(auto it=conversations_.begin();it!=conversations_.end();){
      if(idle(it->first)){evicted.push_back(it->first);it=conversations_.erase(it);}
      else ++it;
    }
    for(auto it=latestStreams_.begin();it!=latestStreams_.end();){
      if(idle(it->first)){streamThrottle_.forget(it->first);it=latestStreams_.erase(it);}
      else ++it;
    }
    std::erase_if(writtenMessageIds_,[&](const auto& item){return idle(item.first);});
    std::erase_if(lastTranscriptUuids_,[&](const auto& item){return idle(item.first);});
  }
  std::erase_if(pendingDirectInputs_,[](const auto& item){return item.second.empty();});
  if(!evicted.empty())setAppState([&](const Json& old){
    Json next=old;
    for(const auto& id:evicted){
      if(!value::property(field(next,"tasks"),id).is_object())continue;
      auto& mission=next["tasks"][id];
      for(const auto* key:{"messages","streamingText","streamingThinking",
          "streamingToolUses","streamMode"})mission.erase(key);
      mission["diskLoaded"]=false;
    }
    return next;
  });
}
void IndigenousBatApplication::sweepSettledMissions(){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Agent record sweep requires owner");
  if(closing_)return;
  const auto tasks=inputs_.store->getState()->fieldRef("tasks");
  if(!tasks)return;
  std::set<std::string> busy;
  {
    std::lock_guard lock(conversationMutex_);
    for(const auto& [id,_]:batJobs_)busy.insert(id);
    busy.insert(pendingResumes_.begin(),pendingResumes_.end());
  }
  for(const auto& [id,inputs]:pendingDirectInputs_)if(!inputs.empty())busy.insert(id);
  busy.insert(pendingContinuationScheduled_.begin(),pendingContinuationScheduled_.end());
  const auto isBusy=[&busy](const std::string& id){return busy.contains(id);};
  const auto now=static_cast<std::int64_t>(platform::nowMillis());
  const auto sweep=bats::sweepSettledBatMissions(*tasks,now,
    bats::settledBatRecordLimit,isBusy);
  if(sweep.missions)setAppState([&](const Json& state){
    const auto current=bats::sweepSettledBatMissions(field(state,"tasks"),now,
      bats::settledBatRecordLimit,isBusy);
    if(!current.missions)return state;
    Json next=state;next["tasks"]=*current.missions;return next;
  });
  if(!sweep.nextDueMs||(sweepDueMs_&&*sweepDueMs_<=*sweep.nextDueMs))return;
  sweepTimer_.cancel();sweepDueMs_=sweep.nextDueMs;
  // Unreferenced: a pending sweep never keeps the application alive.
  sweepTimer_=inputs_.owner.setTimeout(static_cast<double>(*sweep.nextDueMs-now+25),
    [weak=weak_from_this()]{
      const auto self=weak.lock();if(!self)return;
      self->sweepDueMs_.reset();self->sweepTimer_={};
      self->sweepSettledMissions();
    },false);
}
event_loop::Deferred<std::size_t> IndigenousBatApplication::seedSavedBats(){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Saved Bat listing requires owner");
  event_loop::Deferred<std::size_t> output(inputs_.owner);
  try{
    if(closing_)throw std::runtime_error("Agent application is shutting down");
    const auto flight=inputs_.currentFlightId();
    // Each Flight is listed once; later conversation id changes for the same
    // Flight do not rescan its Bats.
    if(!seededFlights_.insert(flight).second){output.resolve(0);return output;}
    std::string directory;
    {
      std::lock_guard lock(pathsMutex_);
      // The probe name only locates this Flight's subagents directory.
      directory=value::dirname(pathsForFlightLocked(flight)->getBatTranscriptPath("probe"));
    }
    auto self=shared_from_this();++conversationJobs_;
    application::runInWorker<std::vector<flights::SavedBat>>(inputs_.owner,[directory]{
      return flights::scanSavedBats(runtime::pathFromUtf8(directory),
        bats::settledBatRecordLimit);
    }).then([self,flight,output](const std::vector<flights::SavedBat>* found,
        const std::exception_ptr& error){
      --self->conversationJobs_;self->checkShutdown();
      if(error){self->seededFlights_.erase(flight);output.reject(error);return;}
      try{
        if(!found||self->closing_){output.resolve(0);return;}
        if(self->inputs_.currentFlightId()!=flight){
          // Seed it again if this Flight becomes current later.
          self->seededFlights_.erase(flight);output.resolve(0);return;
        }
        output.resolve(self->applySavedBats(flight,*found));
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
std::size_t IndigenousBatApplication::applySavedBats(const std::string& flight,
    const std::vector<flights::SavedBat>& found){
  // /clear or a resume may have moved to another Flight while the scan ran;
  // its Bats belong to the Flight they were found in.
  if(found.empty()||inputs_.currentFlightId()!=flight)return 0;
  std::size_t added=0;
  {
    std::lock_guard lock(pathsMutex_);
    auto paths=pathsForFlightLocked(flight);
    for(const auto& bat:found){
      if(batFlights_.try_emplace(bat.id,flight).second&&!bat.subdirectory.empty())
        paths->setBatTranscriptSubdir(bat.id,bat.subdirectory);
    }
  }
  setAppState([&](const Json& state){
    Json next=state;
    if(!next["tasks"].is_object())next["tasks"]=Json::object();
    for(const auto& bat:found){
      if(!value::property(next["tasks"],bat.id).is_null())continue;
      next["tasks"][bat.id]=bats::savedBatRecord(bat.id,bat.description,
        bat.agentType,bat.poetry,bat.startTime,bat.endTime,bat.gadgetUses,
        bat.lastAssistant,registration_.getMissionOutputPath(bat.id));
      ++added;
    }
    return added?next:state;
  });
  return added;
}
Json IndigenousBatApplication::retainedStateSummary()const{
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Agent memory snapshot requires owner");
  std::lock_guard lock(conversationMutex_);
  std::size_t bytes=0,ids=0;
  for(const auto& [_,value]:latestStreams_)bytes+=textBytes(value);
  for(const auto& [_,values]:writtenMessageIds_)ids+=values.size();
  return Json{{"retainedChildContexts",conversations_.size()},
    {"retainedStreamBytes",bytes},{"transcriptDedupIds",ids}};
}
std::unordered_map<std::string,std::string> IndigenousBatApplication::transcriptOrigins()const{
  std::lock_guard lock(pathsMutex_);return batFlights_;
}
void IndigenousBatApplication::restoreTranscriptOrigins(
    const std::unordered_map<std::string,std::string>& origins){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Agent origins require owner");
  std::lock_guard lock(pathsMutex_);
  batFlights_.insert(origins.begin(),origins.end());
}
} // namespace batbox::application_bats
