#include "normalization/Internal.hpp"
#include <set>
#include <vector>

namespace batbox::normalization {
using namespace internal;
namespace {
// value::trim(text.get<std::string>()).empty() without copying the text.
bool blankText(const Json& text) {
  if(!text.is_string()) return value::trim(text.get<std::string>()).empty();
  return value::trimView(text.get_ref<const std::string&>()).empty();
}
} // namespace

Json filterOrphanedThinkingOnlyMessages(Json messages) {
  std::set<std::string> idsWithNonThinkingContent;
  for(const auto& msg:messages) {
    if(!typeIs(msg,"assistant")) continue;
    const Json& content=get(get(msg,"message"),"content");
    if(!content.is_array()) continue;
    bool hasNonThinking=false;
    for(const auto& block:content)
      if(!typeIs(block,"thinking") && !typeIs(block,"redacted_thinking")) hasNonThinking=true;
    const Json& id=get(get(msg,"message"),"id");
    if(hasNonThinking && value::truthy(id)) idsWithNonThinkingContent.insert(id.get<std::string>());
  }
  Json filtered=Json::array();
  for(auto& msg:messages) {
    if(!typeIs(msg,"assistant")) {filtered.push_back(std::move(msg));continue;}
    const Json& content=get(get(msg,"message"),"content");
    if(!content.is_array() || content.empty()) {filtered.push_back(std::move(msg));continue;}
    bool allThinking=true;
    for(const auto& block:content) if(!typeIs(block,"thinking") && !typeIs(block,"redacted_thinking")) allThinking=false;
    if(!allThinking) {filtered.push_back(std::move(msg));continue;}
    const Json& id=get(get(msg,"message"),"id");
    if(value::truthy(id) && idsWithNonThinkingContent.contains(id.get<std::string>())) filtered.push_back(std::move(msg));
  }
  return filtered;
}

Json filterTrailingThinkingFromLastAssistant(Json messages) {
  if(messages.empty() || !typeIs(messages.back(),"assistant")) return messages;
  const Json& content=get(get(messages.back(),"message"),"content");
  if(content.empty() || (!typeIs(content.back(),"thinking") && !typeIs(content.back(),"redacted_thinking"))) return messages;
  int lastValidIndex=static_cast<int>(content.size())-1;
  while(lastValidIndex>=0 && (typeIs(content[static_cast<std::size_t>(lastValidIndex)],"thinking") ||
        typeIs(content[static_cast<std::size_t>(lastValidIndex)],"redacted_thinking"))) --lastValidIndex;
  Json filteredContent=Json::array();
  if(lastValidIndex<0) filteredContent.push_back(Json{{"type","text"},{"text","[No message content]"},{"citations",Json::array()}});
  else for(int i=0;i<=lastValidIndex;++i) filteredContent.push_back(content[static_cast<std::size_t>(i)]);
  messages.back()["message"]["content"]=std::move(filteredContent);return messages;
}

Json filterWhitespaceOnlyAssistantMessages(Json messages) {
  std::vector<bool> keep;keep.reserve(messages.size());bool hasChanges=false;
  for(const auto& message:messages) {
    if(!typeIs(message,"assistant")) {keep.push_back(true);continue;}
    const Json& content=get(get(message,"message"),"content");
    if(!content.is_array() || content.empty()) {keep.push_back(true);continue;}
    bool onlyWhitespace=true;
    for(const auto& block:content) {
      if(!typeIs(block,"text")) {onlyWhitespace=false;break;}
      const Json& text=get(block,"text");
      if(!text.is_null() && !blankText(text)) {onlyWhitespace=false;break;}
    }
    if(onlyWhitespace) hasChanges=true;
    keep.push_back(!onlyWhitespace);
  }
  if(!hasChanges) return messages;
  Json filtered=Json::array();std::size_t index=0;
  for(auto& message:messages) if(keep[index++]) filtered.push_back(std::move(message));
  return mergeAdjacentUserMessages(std::move(filtered));
}

Json ensureNonEmptyAssistantContent(Json messages,const Dependencies& deps) {
  Json& result=messages;
  for(std::size_t index=0;index<result.size();++index) {
    Json& message=result[index];
    if(!typeIs(message,"assistant") || index==result.size()-1) continue;
    const Json& content=get(get(message,"message"),"content");
    if(content.is_array() && content.empty())
      message["message"]["content"]=Json::array({Json{{"type","text"},{"text",deps.noContentMessage},{"citations",Json::array()}}});
  }
  return messages;
}

Json sanitizeErrorGadgetResultContent(Json messages) {
  for(auto& msg:messages) {
    if(!typeIs(msg,"user")) continue;
    const Json& content=get(get(msg,"message"),"content");
    if(!content.is_array()) continue;
    // Only error results that hold a non-text item change; they are rewritten
    // where they stand inside the message, which nothing else shares.
    for(auto& b:msg["message"]["content"]) {
      if(!typeIs(b,"tool_result") || !value::truthy(get(b,"is_error"))) continue;
      const Json& trContent=get(b,"content");
      if(!trContent.is_array()) continue;
      bool allText=true;for(const auto& c:trContent) if(!typeIs(c,"text")) allText=false;
      if(allText) continue;
      std::string joined;bool first=true;
      for(const auto& c:trContent) if(typeIs(c,"text")) {
        if(!first) joined+="\n\n";
        joined+=get(c,"text").get<std::string>();
        first=false;
      }
      b["content"]=first?Json::array():Json::array({textBlock(joined)});
    }
  }
  return messages;
}

Json stripAdvisorBlocks(Json messages) {
  const auto advisorBlock=[](const Json& b){
    return typeIs(b,"advisor_tool_result") || (typeIs(b,"server_tool_use") && get(b,"name")=="advisor");
  };
  for(auto& msg:messages) {
    if(!typeIs(msg,"assistant")) continue;
    const Json& content=get(get(msg,"message"),"content");
    bool any=false;for(const auto& b:content) if(advisorBlock(b)) {any=true;break;}
    if(!any) continue;
    Json filtered=Json::array();
    for(const auto& b:content) {
      if(advisorBlock(b)) continue;
      filtered.push_back(b);
    }
    bool onlyNonVisible=filtered.empty();
    if(!filtered.empty()) {
      onlyNonVisible=true;
      for(const auto& b:filtered) {
        if(typeIs(b,"thinking") || typeIs(b,"redacted_thinking")) continue;
        if(typeIs(b,"text") && (get(b,"text").is_null() || blankText(get(b,"text")))) continue;
        onlyNonVisible=false;break;
      }
    }
    if(onlyNonVisible) filtered.push_back(Json{{"type","text"},{"text","[Advisor response]"},{"citations",Json::array()}});
    msg["message"]["content"]=std::move(filtered);
  }
  return messages;
}

} // namespace batbox::normalization
