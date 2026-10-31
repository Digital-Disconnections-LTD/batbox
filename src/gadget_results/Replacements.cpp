#include "gadget_results/Budget.hpp"
#include "runtime/MessageSnapshot.hpp"

namespace batbox::gadget_results {
MessageArray makeMessageArray(const Json& messages){
  auto output=std::make_shared<std::vector<MessageRef>>();
  for(const auto& message:messages)output->push_back(std::make_shared<Json>(message));
  return output;
}
Json snapshotMessages(const MessageArray& messages){
  Json output=Json::array();for(const auto& message:*messages)output.push_back(runtime::snapshotMessage(message));return output;
}
MessageArray replaceGadgetResultContents(const MessageArray& messages,const ReplacementMap& replacements){
  auto output=std::make_shared<std::vector<MessageRef>>();output->reserve(messages->size());
  for(const auto& reference:*messages){
    auto message=runtime::snapshotMessage(reference);
    if(value::property(message,"type")!="user"||!value::property(value::property(message,"message"),"content").is_array()){
      output->push_back(reference);continue;
    }
    auto& content=message["message"]["content"];bool needsReplace=false;
    for(const auto& block:content)if(value::property(block,"type")=="tool_result"&&
      replacements.contains(block.at("tool_use_id")))needsReplace=true;
    if(!needsReplace){output->push_back(reference);continue;}
    for(auto& block:content)if(value::property(block,"type")=="tool_result"){
      if(const auto* replacement=replacements.get(block.at("tool_use_id")))block["content"]=*replacement;
    }
    output->push_back(std::make_shared<Json>(std::move(message)));
  }
  return output;
}
Json replaceGadgetResultContents(const Json& messages,const ReplacementMap& replacements){
  return snapshotMessages(replaceGadgetResultContents(makeMessageArray(messages),replacements));
}
std::shared_ptr<ContentReplacementState> reconstructContentReplacementState(
  const Json& messages,const Json& records,const ReplacementMap* inherited){
  auto state=createContentReplacementState();SeenIds candidates;
  for(const auto& group:collectCandidatesByMessage(messages))for(const auto& candidate:group)candidates.add(candidate.gadgetUseId);
  for(const auto& id:candidates.entries())state->seenIds.add(id);
  for(const auto& record:records){
    if(value::property(record,"kind")!="tool-result")continue;
    const auto id=record.at("toolUseId").get<std::string>();
    if(candidates.contains(id))state->replacements.set(id,record.at("replacement"));
  }
  if(inherited)for(const auto& [id,replacement]:inherited->entries()){
    if(candidates.contains(id)&&!state->replacements.contains(id))state->replacements.set(id,replacement);
  }
  return state;
}
std::shared_ptr<ContentReplacementState> reconstructForSubagentResume(
  const std::shared_ptr<ContentReplacementState>& parent,const Json& messages,const Json& records){
  return parent?reconstructContentReplacementState(messages,records,&parent->replacements):nullptr;
}
std::shared_ptr<ContentReplacementState> provisionContentReplacementState(bool enabled,
  const std::optional<Json>& initial,const Json& records){
  if(!enabled)return {};
  return initial?reconstructContentReplacementState(*initial,records):createContentReplacementState();
}
} // namespace batbox::gadget_results
