#include "application_bats/IndigenousBatApplication.hpp"
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
    std::erase_if(latestStreams_,[&](const auto& item){return idle(item.first);});
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
