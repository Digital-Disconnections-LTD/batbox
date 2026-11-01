#include "flights/Transcript.hpp"
#include <algorithm>
#include <limits>
namespace batbox::flights {
MessageMap::MessageMap(const Json& messages) {
  for(const auto& message:messages)set(message.at("uuid"),message);
}
MessageMap::MessageMap(const MessageMap& source){
  // Rebuild iterator ownership; copying the index would point into source.
  for(const auto& value:source.entries)entries.push_back(value);
  auto copied=entries.begin();
  std::unordered_map<const Json*,std::list<Json>::iterator> nodes;
  for(const auto& original:source.entries){nodes.emplace(&original,copied);++copied;}
  for(const auto& [uuid,position]:source.positions)positions.emplace(uuid,nodes.at(&*position));
}
MessageMap& MessageMap::operator=(const MessageMap& source){
  if(this!=&source){MessageMap copy(source);entries.swap(copy.entries);positions.swap(copy.positions);}return *this;
}
std::vector<std::string> MessageMap::keys()const{
  std::unordered_map<const Json*,const std::string*> keyFor;
  for(const auto& [key,position]:positions)keyFor.emplace(&*position,&key);
  std::vector<std::string> keys;keys.reserve(entries.size());
  for(const auto& entry:entries)keys.push_back(*keyFor.at(&entry));
  return keys;
}
void MessageMap::set(const std::string& uuid,Json message) {
  if(const auto at=positions.find(uuid);at!=positions.end())*at->second=std::move(message);
  else {entries.push_back(std::move(message));positions.emplace(uuid,std::prev(entries.end()));}
}
const Json* MessageMap::get(const std::string& uuid)const {
  const auto at=positions.find(uuid);return at==positions.end() ? nullptr : &*at->second;
}
Json* MessageMap::get(const std::string& uuid) {
  const auto at=positions.find(uuid);return at==positions.end() ? nullptr : &*at->second;
}
bool MessageMap::erase(const std::string& uuid) {
  const auto at=positions.find(uuid);if(at==positions.end())return false;
  entries.erase(at->second);positions.erase(at);
  return true;
}
std::optional<Json> findLatestMessage(const Json& messages,const std::function<bool(const Json&)>& predicate,
                                    GraphServices& services) {
  std::optional<Json> latest;double maximum=-std::numeric_limits<double>::infinity();
  for(const auto& message:messages) {
    if(!predicate(message))continue;
    // A missing or malformed timestamp sorts oldest instead of failing the
    // whole load; the chain is still rebuilt from parentUuid links.
    const auto& stamp=value::property(message,"timestamp");
    const auto time=stamp.is_string()?services.parseTimestamp(stamp.get<std::string>()):
      std::numeric_limits<double>::lowest();
    if(time>maximum){maximum=time;latest=message;}
  }
  return latest;
}
Json recoverOrphanedParallelGadgetResults(const MessageMap& messages,const Json& chain,
                                       std::set<std::string>& seen,GraphServices& services) {
  std::vector<Json> assistants;
  for(const auto& m:chain)if(m.at("type")=="assistant")assistants.push_back(m);
  if(assistants.empty())return chain;
  std::unordered_map<std::string,std::string> anchors;
  for(const auto& m:assistants) {
    const auto& id=value::property(m.at("message"),"id");
    if(value::truthy(id))anchors[id.get<std::string>()]=m.at("uuid");
  }
  std::unordered_map<std::string,std::vector<Json>> siblings,results;
  for(const auto& m:messages.values()) {
    const auto& type=m.at("type");
    const auto& body=value::property(m,"message");
    const auto& id=value::property(body,"id");
    if(type=="assistant" && value::truthy(id))siblings[id.get<std::string>()].push_back(m);
    else if(type=="user" && value::truthy(value::property(m,"parentUuid")) && value::property(body,"content").is_array()) {
      const auto& content=body.at("content");
      if(std::any_of(content.begin(),content.end(),[](const Json& b){return b.at("type")=="tool_result";}))
        results[m.at("parentUuid").get<std::string>()].push_back(m);
    }
  }
  std::set<std::string> processed;
  std::unordered_map<std::string,std::vector<Json>> inserts;
  std::size_t recoveredCount=0;
  for(const auto& assistant:assistants) {
    const auto& rawId=value::property(assistant.at("message"),"id");
    if(!value::truthy(rawId))continue;
    const auto id=rawId.get<std::string>();
    if(processed.contains(id))continue;
    processed.insert(id);
    const auto group=siblings.contains(id) ? siblings.at(id) : std::vector<Json>{assistant};
    std::vector<Json> orphanedSiblings,orphanedResults;
    for(const auto& member:group) {
      if(!seen.contains(member.at("uuid").get<std::string>()))orphanedSiblings.push_back(member);
      const auto at=results.find(member.at("uuid").get<std::string>());
      if(at!=results.end())for(const auto& result:at->second)
        if(!seen.contains(result.at("uuid").get<std::string>()))orphanedResults.push_back(result);
    }
    if(orphanedSiblings.empty() && orphanedResults.empty())continue;
    const auto stampOf=[](const Json& value)->std::string{
      const auto& stamp=value::property(value,"timestamp");
      return stamp.is_string()?stamp.get<std::string>():std::string();};
    const auto compare=[&](const Json& a,const Json& b){return services.localeCompare(stampOf(a),stampOf(b))<0;};
    std::stable_sort(orphanedSiblings.begin(),orphanedSiblings.end(),compare);
    std::stable_sort(orphanedResults.begin(),orphanedResults.end(),compare);
    orphanedSiblings.insert(orphanedSiblings.end(),orphanedResults.begin(),orphanedResults.end());
    for(const auto& m:orphanedSiblings)seen.insert(m.at("uuid").get<std::string>());
    recoveredCount+=orphanedSiblings.size();
    inserts[anchors.at(id)]=std::move(orphanedSiblings);
  }
  if(recoveredCount==0)return chain;
  Json result=Json::array();
  for(const auto& message:chain) {
    result.push_back(message);
    const auto at=inserts.find(message.at("uuid").get<std::string>());
    if(at!=inserts.end())for(const auto& m:at->second)result.push_back(m);
  }
  return result;
}
Json buildConversationChain(const MessageMap& messages,const Json& leaf,GraphServices& services) {
  Json transcript=Json::array();std::set<std::string> seen;
  const Json* current=&leaf;
  while(current) {
    const auto uuid=current->at("uuid").get<std::string>();
    if(seen.contains(uuid)) {
      services.logError("Cycle detected in parentUuid chain at message "+uuid+". Returning partial transcript.");
      break;
    }
    seen.insert(uuid);transcript.push_back(*current);
    const auto& parent=value::property(*current,"parentUuid");
    current=value::truthy(parent) ? messages.get(parent.get<std::string>()) : nullptr;
  }
  std::reverse(transcript.begin(),transcript.end());
  return recoverOrphanedParallelGadgetResults(messages,transcript,seen,services);
}
}
