#include "compaction/FlightPolicy.hpp"
#include <set>

namespace batbox::compaction {
namespace {
const Json& property(const Json& source,const char* name){
  return value::property(source,name);
}
bool typeIs(const Json& source,const char* name){
  return value::stringEquals(property(source,"type"),name);
}
// Same gadget whitelist the time-based microcompact pass uses: results whose
// inputs are recorded as tool_use blocks by these names are re-derivable, so
// folding them costs the thread nothing it cannot redo.
bool compactable(const Json& block){
  if(!typeIs(block,"tool_use"))return false;
  const auto& name=property(block,"name");
  if(!name.is_string())return false;
  static const std::set<std::string> names={
    "Read","Bash","PowerShell","Grep","Glob","WebSearch",
    "WebFetch","Edit","Write"};
  return names.contains(name.get<std::string>());
}
const FlightPolicy kMain{FlightKind::Main,true,false,5,0.75};
const FlightPolicy kDispatch{FlightKind::Dispatch,false,true,3,0.75};
const FlightPolicy kAuxiliary{FlightKind::Auxiliary,false,false,3,0.75};
}

FlightKind flightKind(const std::string& querySource){
  if(querySource.rfind("agent:",0)==0)return FlightKind::Dispatch;
  if(querySource=="compact"||querySource=="session_memory")return FlightKind::Auxiliary;
  return FlightKind::Main;
}
const FlightPolicy& flightPolicy(const std::string& querySource){
  switch(flightKind(querySource)){
    case FlightKind::Dispatch:return kDispatch;
    case FlightKind::Auxiliary:return kAuxiliary;
    case FlightKind::Main:return kMain;
  }
  return kMain;
}

PruneResult pruneOldToolResults(const Json& messages,std::size_t keepRecent,
    const std::function<double(const Json&)>& resultTokens,
    const std::string& folded){
  PruneResult out;
  out.messages=messages;
  if(!messages.is_array())return out;
  std::vector<std::string> ids;
  for(const auto& message:messages){
    const auto& content=property(property(message,"message"),"content");
    if(!typeIs(message,"assistant")||!content.is_array())continue;
    for(const auto& block:content)
      if(compactable(block)&&property(block,"id").is_string())
        ids.push_back(property(block,"id").get<std::string>());
  }
  const auto firstKeep=ids.size()>keepRecent?ids.size()-keepRecent:0;
  std::set<std::string> keepSet;
  for(std::size_t i=firstKeep;i<ids.size();++i)keepSet.insert(ids[i]);
  std::set<std::string> foldSet;
  for(std::size_t i=0;i<firstKeep;++i)foldSet.insert(ids[i]);
  if(foldSet.empty())return out;
  for(auto& message:out.messages){
    if(!typeIs(message,"user"))continue;
    if(!message.contains("message")||!message["message"].is_object())continue;
    auto& nested=message["message"];
    if(!nested.contains("content")||!nested["content"].is_array())continue;
    for(auto& block:nested["content"]){
      const auto& gadgetUseId=property(block,"tool_use_id");
      if(typeIs(block,"tool_result")&&gadgetUseId.is_string()&&
          foldSet.contains(gadgetUseId.get<std::string>())&&
          property(block,"content")!=folded){
        out.tokensSaved+=resultTokens(block);
        block["content"]=folded;
        ++out.cleared;
      }
    }
  }
  out.changed=out.cleared>0&&out.tokensSaved>0;
  return out;
}
} // namespace batbox::compaction
