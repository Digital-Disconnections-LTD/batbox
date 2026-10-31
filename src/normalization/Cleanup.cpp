#include "normalization/Internal.hpp"
#include <set>
#include <vector>

namespace batbox::normalization {
using namespace internal;

Json filterOrphanedThinkingOnlyMessages(const Json& messages) {
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
  for(const auto& msg:messages) {
    if(!typeIs(msg,"assistant")) {filtered.push_back(msg);continue;}
    const Json& content=get(get(msg,"message"),"content");
    if(!content.is_array() || content.empty()) {filtered.push_back(msg);continue;}
    bool allThinking=true;
    for(const auto& block:content) if(!typeIs(block,"thinking") && !typeIs(block,"redacted_thinking")) allThinking=false;
    if(!allThinking) {filtered.push_back(msg);continue;}
    const Json& id=get(get(msg,"message"),"id");
    if(value::truthy(id) && idsWithNonThinkingContent.contains(id.get<std::string>())) filtered.push_back(msg);
  }
  return filtered;
}

Json filterTrailingThinkingFromLastAssistant(const Json& messages) {
  if(messages.empty() || !typeIs(messages.back(),"assistant")) return messages;
  const Json& content=get(get(messages.back(),"message"),"content");
  if(content.empty() || (!typeIs(content.back(),"thinking") && !typeIs(content.back(),"redacted_thinking"))) return messages;
  int lastValidIndex=static_cast<int>(content.size())-1;
  while(lastValidIndex>=0 && (typeIs(content[static_cast<std::size_t>(lastValidIndex)],"thinking") ||
        typeIs(content[static_cast<std::size_t>(lastValidIndex)],"redacted_thinking"))) --lastValidIndex;
  Json filteredContent=Json::array();
  if(lastValidIndex<0) filteredContent.push_back(Json{{"type","text"},{"text","[No message content]"},{"citations",Json::array()}});
  else for(int i=0;i<=lastValidIndex;++i) filteredContent.push_back(content[static_cast<std::size_t>(i)]);
  Json result=messages;result.back()["message"]["content"]=std::move(filteredContent);return result;
}

Json filterWhitespaceOnlyAssistantMessages(const Json& messages,const Dependencies& deps) {
  bool hasChanges=false;Json filtered=Json::array();
  for(const auto& message:messages) {
    if(!typeIs(message,"assistant")) {filtered.push_back(message);continue;}
    const Json& content=get(get(message,"message"),"content");
    if(!content.is_array() || content.empty()) {filtered.push_back(message);continue;}
    bool onlyWhitespace=true;
    for(const auto& block:content) {
      if(!typeIs(block,"text")) {onlyWhitespace=false;break;}
      const Json& text=get(block,"text");
      if(!text.is_null() && !value::trim(text.get<std::string>()).empty()) {onlyWhitespace=false;break;}
    }
    if(onlyWhitespace) hasChanges=true;
    else filtered.push_back(message);
  }
  if(!hasChanges) return messages;
  return mergeAdjacentUserMessages(filtered,deps);
}

Json ensureNonEmptyAssistantContent(const Json& messages,const Dependencies& deps) {
  Json result=messages;
  for(std::size_t index=0;index<result.size();++index) {
    Json& message=result[index];
    if(!typeIs(message,"assistant") || index==result.size()-1) continue;
    const Json& content=get(get(message,"message"),"content");
    if(content.is_array() && content.empty())
      message["message"]["content"]=Json::array({Json{{"type","text"},{"text",deps.noContentMessage},{"citations",Json::array()}}});
  }
  return result;
}

Json sanitizeErrorGadgetResultContent(const Json& messages) {
  Json result=Json::array();
  for(const auto& msg:messages) {
    if(!typeIs(msg,"user")) {result.push_back(msg);continue;}
    const Json& content=get(get(msg,"message"),"content");
    if(!content.is_array()) {result.push_back(msg);continue;}
    bool changed=false;Json newContent=Json::array();
    for(const auto& b:content) {
      if(!typeIs(b,"tool_result") || !value::truthy(get(b,"is_error"))) {newContent.push_back(b);continue;}
      const Json& trContent=get(b,"content");
      if(!trContent.is_array()) {newContent.push_back(b);continue;}
      bool allText=true;for(const auto& c:trContent) if(!typeIs(c,"text")) allText=false;
      if(allText) {newContent.push_back(b);continue;}
      changed=true;std::string joined;bool first=true;
      for(const auto& c:trContent) if(typeIs(c,"text")) {
        if(!first) joined+="\n\n";
        joined+=get(c,"text").get<std::string>();
        first=false;
      }
      Json copy=b;copy["content"]=first?Json::array():Json::array({textBlock(joined)});
      newContent.push_back(std::move(copy));
    }
    if(changed){Json copy=msg;copy["message"]["content"]=std::move(newContent);result.push_back(std::move(copy));}
    else result.push_back(msg);
  }
  return result;
}

Json stripAdvisorBlocks(const Json& messages) {
  Json result=Json::array();
  for(const auto& msg:messages) {
    if(!typeIs(msg,"assistant")) {result.push_back(msg);continue;}
    const Json& content=get(get(msg,"message"),"content");
    Json filtered=Json::array();
    for(const auto& b:content) {
      if(typeIs(b,"advisor_tool_result") || (typeIs(b,"server_tool_use") && get(b,"name")=="advisor")) continue;
      filtered.push_back(b);
    }
    if(filtered.size()==content.size()) {result.push_back(msg);continue;}
    bool onlyNonVisible=filtered.empty();
    if(!filtered.empty()) {
      onlyNonVisible=true;
      for(const auto& b:filtered) {
        if(typeIs(b,"thinking") || typeIs(b,"redacted_thinking")) continue;
        if(typeIs(b,"text") && (get(b,"text").is_null() || value::trim(get(b,"text").get<std::string>()).empty())) continue;
        onlyNonVisible=false;break;
      }
    }
    if(onlyNonVisible) filtered.push_back(Json{{"type","text"},{"text","[Advisor response]"},{"citations",Json::array()}});
    Json copy=msg;copy["message"]["content"]=std::move(filtered);result.push_back(std::move(copy));
  }
  return result;
}

} // namespace batbox::normalization

namespace batbox::normalization::internal {

Json smooshSystemReminderSiblings(const Json& messages) {
  Json result=Json::array();
  for(const auto& msg:messages) {
    if(!typeIs(msg,"user")) {result.push_back(msg);continue;}
    const Json& content=get(get(msg,"message"),"content");
    if(!content.is_array()) {result.push_back(msg);continue;}
    bool hasResult=false;for(const auto& b:content) if(typeIs(b,"tool_result")) hasResult=true;
    if(!hasResult) {result.push_back(msg);continue;}
    Json srText=Json::array(),kept=Json::array();
    for(const auto& b:content) {
      if(typeIs(b,"text") && get(b,"text").is_string() &&
         get(b,"text").get<std::string>().rfind("<system-reminder>",0)==0) srText.push_back(b);
      else kept.push_back(b);
    }
    if(srText.empty()) {result.push_back(msg);continue;}
    std::size_t lastResultIndex=0;
    for(std::size_t i=0;i<kept.size();++i) if(typeIs(kept[i],"tool_result")) lastResultIndex=i;
    Json smooshed=smooshIntoGadgetResult(kept[lastResultIndex],srText);
    if(smooshed.is_null()) {result.push_back(msg);continue;}
    kept[lastResultIndex]=std::move(smooshed);
    Json copy=msg;copy["message"]["content"]=std::move(kept);result.push_back(std::move(copy));
  }
  return result;
}

} // namespace batbox::normalization::internal
