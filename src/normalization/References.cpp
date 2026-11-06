#include "normalization/Internal.hpp"
#include <algorithm>
#include <set>
#include <vector>

namespace batbox::normalization {
using namespace internal;

Json stripGadgetReferenceBlocksFromUserMessage(Json message) {
  const Json& content=get(get(message,"message"),"content");
  if(!content.is_array()) return message;
  bool found=false;
  for(const auto& block:content) {
    if(!typeIs(block,"tool_result")) continue;
    const Json& nested=get(block,"content");
    if(nested.is_array()) for(const auto& c:nested) if(typeIs(c,"tool_reference")) found=true;
  }
  if(!found) return message;
  for(auto& block:message["message"]["content"]) {
    const Json& nested=get(block,"content");
    if(!typeIs(block,"tool_result") || !nested.is_array()) continue;
    Json filtered=Json::array();
    for(const auto& c:nested) if(!typeIs(c,"tool_reference")) filtered.push_back(c);
    if(filtered.empty()) filtered.push_back(textBlock("[Tool references removed - tool search not enabled]"));
    block["content"]=std::move(filtered);
  }
  return message;
}

Json stripCallerFieldFromAssistantMessage(Json message) {
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
  message["message"]["content"]=std::move(mapped);return message;
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

} // namespace batbox::normalization::internal
