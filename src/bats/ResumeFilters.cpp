#include "bats/ResumeFilters.hpp"
#include <unordered_map>
#include <unordered_set>

namespace batbox::bats {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
}
Json filterUnresolvedGadgetUses(const Json& messages){
  std::unordered_set<std::string> uses,results;
  for(const auto& message:messages){
    const auto& type=field(message,"type");
    if(type!="user"&&type!="assistant")continue;
    const auto& content=field(field(message,"message"),"content");
    if(!content.is_array())continue;
    for(const auto& block:content){
      if(field(block,"type")=="tool_use"&&field(block,"id").is_string())
        uses.insert(field(block,"id").get<std::string>());
      if(field(block,"type")=="tool_result"&&field(block,"tool_use_id").is_string())
        results.insert(field(block,"tool_use_id").get<std::string>());
    }
  }
  std::unordered_set<std::string> unresolved;
  for(const auto& id:uses)if(!results.contains(id))unresolved.insert(id);
  if(unresolved.empty())return messages;
  Json output=Json::array();
  for(const auto& message:messages){
    const auto& content=field(field(message,"message"),"content");
    bool any=false,allUnresolved=true;
    if(field(message,"type")=="assistant"&&content.is_array())
      for(const auto& block:content)if(field(block,"type")=="tool_use"){
        any=true;
        const auto& id=field(block,"id");
        if(!id.is_string()||!unresolved.contains(id.get<std::string>()))allUnresolved=false;
      }
    if(!any||!allUnresolved)output.push_back(message);
  }
  return output;
}
Json mergeBatTranscriptMessages(const Json& prefix,const Json& live){
  Json output=Json::array();
  std::unordered_map<std::string,std::size_t> positions;
  const auto append=[&](const Json& values){
    if(!values.is_array())return;
    for(const auto& message:values){
      const auto& uuid=field(message,"uuid");
      if(uuid.is_string()){
        const auto id=uuid.get<std::string>();
        if(const auto at=positions.find(id);at!=positions.end()){
          output[at->second]=message;continue;
        }
        positions.emplace(id,output.size());
      }
      output.push_back(message);
    }
  };
  append(prefix);append(live);return output;
}
} // namespace batbox::bats
