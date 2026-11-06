#include "flights/Transcript.hpp"
namespace batbox::flights {
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
