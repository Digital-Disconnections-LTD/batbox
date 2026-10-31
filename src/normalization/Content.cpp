#include "normalization/Internal.hpp"
#include <algorithm>
#include <vector>

namespace batbox::normalization::internal {

Json normalizeUserTextContent(const Json& content) {
  return content.is_string() ? Json::array({textBlock(content.get<std::string>())}) : content;
}

Json hoistGadgetResults(const Json& content) {
  Json gadgetResults=Json::array(), otherBlocks=Json::array();
  for(const auto& block:content) {
    if(typeIs(block,"tool_result")) gadgetResults.push_back(block);
    else otherBlocks.push_back(block);
  }
  for(const auto& block:otherBlocks) gadgetResults.push_back(block);
  return gadgetResults;
}

Json joinTextAtSeam(const Json& a,const Json& b) {
  Json result=Json::array();
  if(!a.empty() && !b.empty() && typeIs(a.back(),"text") && typeIs(b.front(),"text")) {
    for(std::size_t i=0;i+1<a.size();++i) result.push_back(a[i]);
    Json lastA=a.back();
    lastA["text"]=get(lastA,"text").get<std::string>()+"\n";
    result.push_back(std::move(lastA));
  } else for(const auto& block:a) result.push_back(block);
  for(const auto& block:b) result.push_back(block);
  return result;
}

Json smooshIntoGadgetResult(const Json& tr,const Json& incoming) {
  if(incoming.empty()) return tr;
  const Json& existing=get(tr,"content");
  if(existing.is_array()) for(const auto& item:existing) if(typeIs(item,"tool_reference")) return nullptr;
  Json blocks=incoming;
  if(value::truthy(get(tr,"is_error"))) {
    Json filtered=Json::array();
    for(const auto& block:blocks) if(typeIs(block,"text")) filtered.push_back(block);
    blocks=std::move(filtered);
    if(blocks.empty()) return tr;
  }
  const bool allText=std::all_of(blocks.begin(),blocks.end(),[](const Json& block){return typeIs(block,"text");});
  if(allText && (!has(tr,"content") || existing.is_string())) {
    std::vector<std::string> parts;
    if(existing.is_string()) {auto s=value::trim(existing.get<std::string>());if(!s.empty()) parts.push_back(s);}
    for(const auto& block:blocks) {auto s=value::trim(get(block,"text").get<std::string>());if(!s.empty()) parts.push_back(s);}
    std::string joined;
    for(const auto& part:parts){if(!joined.empty()) joined+="\n\n";joined+=part;}
    Json result=tr;result["content"]=joined;return result;
  }
  Json base=Json::array();
  if(has(tr,"content")) {
    if(existing.is_string()) {
      const std::string trimmed=value::trim(existing.get<std::string>());
      if(!trimmed.empty()) base.push_back(textBlock(trimmed));
    } else if(existing.is_array()) base=existing;
  }
  for(const auto& block:blocks) base.push_back(block);
  Json merged=Json::array();
  for(const auto& block:base) {
    if(typeIs(block,"text")) {
      const std::string value=value::trim(get(block,"text").get<std::string>());
      if(value.empty()) continue;
      if(!merged.empty() && typeIs(merged.back(),"text")) {
        Json previous=merged.back();previous["text"]=get(previous,"text").get<std::string>()+"\n\n"+value;
        merged.back()=std::move(previous);
      } else merged.push_back(textBlock(value));
    } else merged.push_back(block);
  }
  Json result=tr;result["content"]=std::move(merged);return result;
}

Json mergeUserContentBlocks(const Json& a,const Json& b,const Dependencies& deps) {
  Json combined=a;
  for(const auto& block:b) combined.push_back(block);
  if(a.empty() || !typeIs(a.back(),"tool_result")) return combined;
  need(static_cast<bool>(deps.featureGate),"featureGate");
  if(!deps.featureGate("tengu_chair_sermon")) {
    const Json& lastBlock=a.back();
    bool allText=true;for(const auto& block:b) if(!typeIs(block,"text")) allText=false;
    if(get(lastBlock,"content").is_string() && allText) {
      combined=a;combined.back()=smooshIntoGadgetResult(lastBlock,b);return combined;
    }
    return combined;
  }
  Json toSmoosh=Json::array(),gadgetResults=Json::array();
  for(const auto& block:b) {
    if(typeIs(block,"tool_result")) gadgetResults.push_back(block);
    else toSmoosh.push_back(block);
  }
  if(toSmoosh.empty()) return combined;
  Json smooshed=smooshIntoGadgetResult(a.back(),toSmoosh);
  if(smooshed.is_null()) return combined;
  Json result=Json::array();
  for(std::size_t i=0;i+1<a.size();++i) result.push_back(a[i]);
  result.push_back(std::move(smooshed));
  for(const auto& block:gadgetResults) result.push_back(block);
  return result;
}

} // namespace batbox::normalization::internal

namespace batbox::normalization {
using namespace internal;

Json mergeUserMessagesAndGadgetResults(const Json& a,const Json& b,const Dependencies& deps) {
  const Json lastContent=normalizeUserTextContent(get(get(a,"message"),"content"));
  const Json currentContent=normalizeUserTextContent(get(get(b,"message"),"content"));
  Json result=a;
  result["message"]["content"]=hoistGadgetResults(mergeUserContentBlocks(lastContent,currentContent,deps));
  return result;
}

Json mergeAssistantMessages(const Json& a,const Json& b) {
  Json result=a;
  Json content=get(get(a,"message"),"content");
  for(const auto& block:get(get(b,"message"),"content")) content.push_back(block);
  result["message"]["content"]=std::move(content);
  return result;
}

Json mergeUserMessages(const Json& a,const Json& b,const Dependencies& deps) {
  const Json lastContent=normalizeUserTextContent(get(get(a,"message"),"content"));
  const Json currentContent=normalizeUserTextContent(get(get(b,"message"),"content"));
  Json result=a;
  if(deps.historySnipFeature && deps.historySnipRuntimeEnabled) {
    if(value::truthy(get(a,"isMeta")) && value::truthy(get(b,"isMeta"))) result["isMeta"]=true;
    else result.erase("isMeta");
  }
  if(value::truthy(get(a,"isMeta"))) {
    if(has(b,"uuid")) result["uuid"]=b["uuid"];
    else result.erase("uuid");
  }
  result["message"]["content"]=hoistGadgetResults(joinTextAtSeam(lastContent,currentContent));
  return result;
}

} // namespace batbox::normalization

namespace batbox::normalization::internal {

Json mergeAdjacentUserMessages(const Json& messages,const Dependencies& deps) {
  Json out=Json::array();
  for(const auto& message:messages) {
    if(typeIs(message,"user") && !out.empty() && typeIs(out.back(),"user"))
      out.back()=mergeUserMessages(out.back(),message,deps);
    else out.push_back(message);
  }
  return out;
}

} // namespace batbox::normalization::internal
