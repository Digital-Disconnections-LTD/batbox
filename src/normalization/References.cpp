#include "normalization/Internal.hpp"
#include <algorithm>
#include <set>
#include <vector>

namespace batbox::normalization {
using namespace internal;

Json reorderAttachmentsForAPI(const Json& messages) {
  Json result=Json::array();
  std::vector<Json> pendingAttachments;
  for(int i=static_cast<int>(messages.size())-1;i>=0;--i) {
    const Json& message=messages[static_cast<std::size_t>(i)];
    if(typeIs(message,"attachment")) {pendingAttachments.push_back(message);continue;}
    const Json& content=get(get(message,"message"),"content");
    const bool stoppingPoint=typeIs(message,"assistant") ||
      (typeIs(message,"user") && content.is_array() && !content.empty() && typeIs(content[0],"tool_result"));
    if(stoppingPoint && !pendingAttachments.empty()) {
      for(const auto& attachment:pendingAttachments) result.push_back(attachment);
      result.push_back(message);pendingAttachments.clear();
    } else result.push_back(message);
  }
  for(const auto& attachment:pendingAttachments) result.push_back(attachment);
  std::reverse(result.begin(),result.end());
  return result;
}

Json stripGadgetReferenceBlocksFromUserMessage(const Json& message) {
  const Json& content=get(get(message,"message"),"content");
  if(!content.is_array()) return message;
  bool found=false;
  for(const auto& block:content) {
    if(!typeIs(block,"tool_result")) continue;
    const Json& nested=get(block,"content");
    if(nested.is_array()) for(const auto& c:nested) if(typeIs(c,"tool_reference")) found=true;
  }
  if(!found) return message;
  Json result=message, mapped=Json::array();
  for(const auto& block:content) {
    const Json& nested=get(block,"content");
    if(!typeIs(block,"tool_result") || !nested.is_array()) {mapped.push_back(block);continue;}
    Json filtered=Json::array();
    for(const auto& c:nested) if(!typeIs(c,"tool_reference")) filtered.push_back(c);
    if(filtered.empty()) filtered.push_back(textBlock("[Tool references removed - tool search not enabled]"));
    Json copy=block;copy["content"]=std::move(filtered);mapped.push_back(std::move(copy));
  }
  result["message"]["content"]=std::move(mapped);
  return result;
}

Json stripCallerFieldFromAssistantMessage(const Json& message) {
  const Json& content=get(get(message,"message"),"content");
  bool found=false;
  for(const auto& block:content) if(typeIs(block,"tool_use") && has(block,"caller") && !block["caller"].is_null()) found=true;
  if(!found) return message;
  Json mapped=Json::array();
  for(const auto& block:content) {
    if(!typeIs(block,"tool_use")) {mapped.push_back(block);continue;}
    Json copy={{"type","tool_use"}};
    for(const auto* key:{"id","name","input"}) if(has(block,key)) copy[key]=block[key];
    mapped.push_back(std::move(copy));
  }
  Json result=message;result["message"]["content"]=std::move(mapped);return result;
}

Json stripUnavailableGadgetReferencesFromUserMessage(const Json& message,
                                                    const std::set<std::string>& availableGadgetNames,
                                                    const Dependencies& deps) {
  const Json& content=get(get(message,"message"),"content");
  if(!content.is_array()) return message;
  bool hasUnavailable=false;
  for(const auto& block:content) {
    const Json& nested=get(block,"content");
    if(!typeIs(block,"tool_result") || !nested.is_array()) continue;
    for(const auto& c:nested) {
      if(!typeIs(c,"tool_reference")) continue;
      const Json& raw=get(c,"tool_name");
      if(!value::truthy(raw)) continue;
      need(static_cast<bool>(deps.normalizeLegacyGadgetName),"normalizeLegacyToolName");
      if(!availableGadgetNames.contains(deps.normalizeLegacyGadgetName(raw.get<std::string>()))) hasUnavailable=true;
    }
  }
  if(!hasUnavailable) return message;
  Json mapped=Json::array();
  for(const auto& block:content) {
    const Json& nested=get(block,"content");
    if(!typeIs(block,"tool_result") || !nested.is_array()) {mapped.push_back(block);continue;}
    Json filtered=Json::array();
    for(const auto& c:nested) {
      if(!typeIs(c,"tool_reference")) {filtered.push_back(c);continue;}
      const Json& raw=get(c,"tool_name");
      if(!value::truthy(raw)) {filtered.push_back(c);continue;}
      if(availableGadgetNames.contains(deps.normalizeLegacyGadgetName(raw.get<std::string>()))) filtered.push_back(c);
    }
    if(filtered.empty()) filtered.push_back(textBlock("[Tool references removed - tools no longer available]"));
    Json copy=block;copy["content"]=std::move(filtered);mapped.push_back(std::move(copy));
  }
  Json result=message;result["message"]["content"]=std::move(mapped);return result;
}

} // namespace batbox::normalization

namespace batbox::normalization::internal {

bool contentHasGadgetReference(const Json& content) {
  for(const auto& block:content) {
    const Json& nested=get(block,"content");
    if(typeIs(block,"tool_result") && nested.is_array())
      for(const auto& c:nested) if(typeIs(c,"tool_reference")) return true;
  }
  return false;
}

Json relocateGadgetReferenceSiblings(const Json& messages) {
  Json result=messages;
  for(std::size_t i=0;i<result.size();++i) {
    const Json msg=result[i];
    if(!typeIs(msg,"user")) continue;
    const Json& content=get(get(msg,"message"),"content");
    if(!content.is_array() || !contentHasGadgetReference(content)) continue;
    Json textSiblings=Json::array();
    for(const auto& b:content) if(typeIs(b,"text")) textSiblings.push_back(b);
    if(textSiblings.empty()) continue;
    int targetIndex=-1;
    for(std::size_t j=i+1;j<result.size();++j) {
      const Json& candidate=result[j];
      if(!typeIs(candidate,"user")) continue;
      const Json& cc=get(get(candidate,"message"),"content");
      if(!cc.is_array()) continue;
      bool hasResult=false;for(const auto& b:cc) if(typeIs(b,"tool_result")) hasResult=true;
      if(!hasResult || contentHasGadgetReference(cc)) continue;
      targetIndex=static_cast<int>(j);break;
    }
    if(targetIndex<0) continue;
    Json filtered=Json::array();
    for(const auto& b:content) if(!typeIs(b,"text")) filtered.push_back(b);
    result[i]["message"]["content"]=std::move(filtered);
    Json targetContent=get(get(result[static_cast<std::size_t>(targetIndex)],"message"),"content");
    for(const auto& b:textSiblings) targetContent.push_back(b);
    result[static_cast<std::size_t>(targetIndex)]["message"]["content"]=std::move(targetContent);
  }
  return result;
}

} // namespace batbox::normalization::internal
