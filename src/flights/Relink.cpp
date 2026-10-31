#include "flights/Transcript.hpp"
namespace batbox::flights {
void applyPreservedSegmentRelinks(MessageMap& messages) {
  std::optional<Json> lastSegment;
  long lastSegmentIndex=-1,lastBoundaryIndex=-1,index=0;
  std::unordered_map<std::string,long> entryIndex;
  for(const auto& entry:messages.values()) {
    entryIndex[entry.at("uuid").get<std::string>()]=index;
    if(entry.at("type")=="system" && value::property(entry,"subtype")=="compact_boundary") {
      lastBoundaryIndex=index;
      const auto& segment=value::property(value::property(entry,"compactMetadata"),"preservedSegment");
      if(value::truthy(segment)){lastSegment=segment;lastSegmentIndex=index;}
    }
    ++index;
  }
  if(!lastSegment)return;
  const bool live=lastSegmentIndex==lastBoundaryIndex;
  std::set<std::string> preserved;
  if(live) {
    std::set<std::string> walked;bool reachedHead=false;
    auto* current=messages.get(lastSegment->at("tailUuid").get<std::string>());
    while(current && !walked.contains(current->at("uuid").get<std::string>())) {
      const auto uuid=current->at("uuid").get<std::string>();
      walked.insert(uuid);preserved.insert(uuid);
      if(uuid==lastSegment->at("headUuid").get<std::string>()){reachedHead=true;break;}
      const auto parent=value::property(*current,"parentUuid");
      current=value::truthy(parent) ? messages.get(parent.get<std::string>()) : nullptr;
    }
    if(!reachedHead)return;
  }
  if(live) {
    const auto head=lastSegment->at("headUuid").get<std::string>();
    const auto anchor=lastSegment->at("anchorUuid");
    const auto tail=lastSegment->at("tailUuid");
    if(auto* message=messages.get(head))(*message)["parentUuid"]=anchor;
    // Updating existing Map entries preserves insertion order and storage size.
    for(const auto& value:messages.values()) {
      const auto uuid=value.at("uuid").get<std::string>();
      if(value::property(value,"parentUuid")==anchor && uuid!=head)
        (*messages.get(uuid))["parentUuid"]=tail;
    }
    for(const auto& uuid:preserved) {
      auto* message=messages.get(uuid);
      if(!message || message->at("type")!="assistant")continue;
      auto usage=value::property(message->at("message"),"usage");
      if(!usage.is_object())usage=Json::object();
      for(const auto* field:{"input_tokens","output_tokens","cache_creation_input_tokens","cache_read_input_tokens"})usage[field]=0;
      (*message)["message"]["usage"]=std::move(usage);
    }
  }
  std::vector<std::string> remove;
  for(const auto& message:messages.values()) {
    const auto uuid=message.at("uuid").get<std::string>();
    const auto at=entryIndex.find(uuid);
    if(at!=entryIndex.end() && at->second<lastBoundaryIndex && !preserved.contains(uuid))remove.push_back(uuid);
  }
  for(const auto& uuid:remove)messages.erase(uuid);
}
void applySnipRemovals(MessageMap& messages) {
  std::set<std::string> remove;
  for(const auto& message:messages.values()) {
    const auto& uuids=value::property(value::property(message,"snipMetadata"),"removedUuids");
    if(!value::truthy(uuids))continue;
    for(const auto& uuid:uuids)remove.insert(uuid.get<std::string>());
  }
  if(remove.empty())return;
  std::unordered_map<std::string,Json> deletedParent;
  for(const auto& uuid:remove) {
    const auto* entry=messages.get(uuid);if(!entry)continue;
    deletedParent[uuid]=value::property(*entry,"parentUuid");messages.erase(uuid);
  }
  const auto resolve=[&](const std::string& start) {
    std::vector<std::string> path;Json current=start;
    while(value::truthy(current) && remove.contains(current.get<std::string>())) {
      const auto id=current.get<std::string>();path.push_back(id);
      const auto at=deletedParent.find(id);
      if(at==deletedParent.end()){current=nullptr;break;}
      current=at->second;
    }
    for(const auto& id:path)deletedParent[id]=current;
    return current;
  };
  for(const auto& value:messages.values()) {
    const auto parent=value::property(value,"parentUuid");
    if(!value::truthy(parent) || !remove.contains(parent.get<std::string>()))continue;
    (*messages.get(value.at("uuid").get<std::string>()))["parentUuid"]=resolve(parent.get<std::string>());
  }
}
}
