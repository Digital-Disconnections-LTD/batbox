#include "gadget_results/Budget.hpp"
#include <algorithm>

namespace batbox::gadget_results {
std::unordered_map<std::string,std::string> buildGadgetNameMap(const Json& messages){
  std::unordered_map<std::string,std::string> names;
  for(const auto& message:messages){
    if(value::property(message,"type")!="assistant")continue;
    const auto& content=value::property(value::property(message,"message"),"content");
    if(!content.is_array())continue;
    for(const auto& block:content)if(value::property(block,"type")=="tool_use")
      names[block.at("id").get<std::string>()]=block.at("name").get<std::string>();
  }
  return names;
}
std::vector<Candidate> collectCandidatesFromMessage(const Json& message){
  std::vector<Candidate> candidates;
  if(value::property(message,"type")!="user")return candidates;
  const auto& content=value::property(value::property(message,"message"),"content");
  if(!content.is_array())return candidates;
  for(const auto& block:content){
    const auto& value=value::property(block,"content");
    if(value::property(block,"type")!="tool_result"||!value::truthy(value)||
      isContentAlreadyCompacted(value)||hasImageBlock(value))continue;
    candidates.push_back({block.at("tool_use_id").get<std::string>(),value,contentSize(value)});
  }
  return candidates;
}
std::vector<std::vector<Candidate>> collectCandidatesByMessage(const Json& messages){
  std::vector<std::vector<Candidate>> groups;std::vector<Candidate> current;
  std::unordered_set<std::string> seenAssistantIds;
  const auto flush=[&]{if(!current.empty())groups.push_back(std::move(current));current.clear();};
  for(const auto& message:messages){
    if(value::property(message,"type")=="user"){
      auto candidates=collectCandidatesFromMessage(message);
      current.insert(current.end(),std::make_move_iterator(candidates.begin()),std::make_move_iterator(candidates.end()));
    }else if(value::property(message,"type")=="assistant"){
      // Undefined ids coalesce in the Set; null uses its JSON spelling while the
      // absent-property sentinel remains separate.
      const auto& body=value::property(message,"message");
      const auto id=body.is_object()&&body.contains("id")?value::stringify(body.at("id")):"undefined";
      if(seenAssistantIds.insert(id).second)flush();
    }
  }
  flush();return groups;
}
CandidatePartition partitionByPriorDecision(const std::vector<Candidate>& candidates,
  const ContentReplacementState& state){
  CandidatePartition result;
  for(const auto& candidate:candidates){
    if(const auto* text=state.replacements.get(candidate.gadgetUseId))
      result.mustReapply.push_back({candidate,*text});
    else if(state.seenIds.contains(candidate.gadgetUseId))result.frozen.push_back(candidate);
    else result.fresh.push_back(candidate);
  }
  return result;
}
std::vector<Candidate> selectFreshToReplace(const std::vector<Candidate>& fresh,
  double frozenSize,double limit){
  auto sorted=fresh;std::stable_sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b){return a.size>b.size;});
  double remaining=frozenSize;for(const auto& c:fresh)remaining+=c.size;
  std::vector<Candidate> selected;
  for(const auto& candidate:sorted){if(remaining<=limit)break;selected.push_back(candidate);remaining-=candidate.size;}
  return selected;
}
} // namespace batbox::gadget_results
