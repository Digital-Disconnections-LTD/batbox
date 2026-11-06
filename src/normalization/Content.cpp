#include "normalization/Internal.hpp"
#include <algorithm>
#include <vector>

namespace batbox::normalization::internal {

Json normalizeUserTextContent(Json content) {
  return content.is_string() ? Json::array({textBlock(content.get<std::string>())}) : std::move(content);
}

Json hoistGadgetResults(Json content) {
  Json gadgetResults=Json::array(), otherBlocks=Json::array();
  for(auto& block:content) {
    if(typeIs(block,"tool_result")) gadgetResults.push_back(std::move(block));
    else otherBlocks.push_back(std::move(block));
  }
  for(auto& block:otherBlocks) gadgetResults.push_back(std::move(block));
  return gadgetResults;
}

Json joinTextAtSeam(Json a,const Json& b) {
  if(a.is_array()) {
    if(!a.empty() && !b.empty() && typeIs(a.back(),"text") && typeIs(b.front(),"text"))
      a.back()["text"]=get(a.back(),"text").get<std::string>()+"\n";
    for(const auto& block:b) a.push_back(block);
    return a;
  }
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

Json mergeUserContentBlocks(Json a,const Json& b) {
  const auto append=[&b](Json combined){
    for(const auto& block:b) combined.push_back(block);
    return combined;
  };
  if(a.empty() || !typeIs(a.back(),"tool_result")) return append(std::move(a));
  bool allText=true;for(const auto& block:b) if(!typeIs(block,"text")) allText=false;
  if(get(a.back(),"content").is_string() && allText) {
    a.back()=smooshIntoGadgetResult(a.back(),b);return a;
  }
  return append(std::move(a));
}

} // namespace batbox::normalization::internal

namespace batbox::normalization {
using namespace internal;

namespace {
// get(get(message,"message"),"content") moved out of message; the key stays
// where it was so a later assignment keeps the member order.
Json takeContent(Json& message) {
  if(!message.is_object()) return Json();
  const auto inner=message.find("message");
  if(inner==message.end() || !inner->is_object()) return Json();
  const auto content=inner->find("content");
  return content==inner->end() ? Json() : std::move(*content);
}
} // namespace

Json mergeUserMessagesAndGadgetResults(Json a,const Json& b) {
  Json lastContent=normalizeUserTextContent(takeContent(a));
  const Json currentContent=normalizeUserTextContent(get(get(b,"message"),"content"));
  a["message"]["content"]=hoistGadgetResults(mergeUserContentBlocks(std::move(lastContent),currentContent));
  return a;
}

Json mergeAssistantMessages(Json a,const Json& b) {
  Json content=takeContent(a);
  for(const auto& block:get(get(b,"message"),"content")) content.push_back(block);
  a["message"]["content"]=std::move(content);
  return a;
}

Json mergeUserMessages(Json a,const Json& b) {
  Json lastContent=normalizeUserTextContent(takeContent(a));
  const Json currentContent=normalizeUserTextContent(get(get(b,"message"),"content"));
  const bool metaBefore=value::truthy(get(a,"isMeta"));
  Json& result=a;
  if(metaBefore) {
    if(has(b,"uuid")) result["uuid"]=b["uuid"];
    else result.erase("uuid");
  }
  result["message"]["content"]=hoistGadgetResults(joinTextAtSeam(std::move(lastContent),currentContent));
  return a;
}

} // namespace batbox::normalization

namespace batbox::normalization::internal {

Json mergeAdjacentUserMessages(Json messages) {
  Json out=Json::array();
  for(auto& message:messages) {
    if(typeIs(message,"user") && !out.empty() && typeIs(out.back(),"user"))
      out.back()=mergeUserMessages(std::move(out.back()),message);
    else out.push_back(std::move(message));
  }
  return out;
}

} // namespace batbox::normalization::internal
