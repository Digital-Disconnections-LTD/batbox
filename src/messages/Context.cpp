#include "messages/Context.hpp"
namespace batbox::messages {
Json getLastAssistantMessage(const Json& values) {
  for (auto it=values.rbegin();it!=values.rend();++it) if (value::property(*it,"type")=="assistant") return *it;
  return nullptr;
}
std::optional<std::string> getAssistantMessageText(const Json& message) {
  if (value::property(message,"type")!="assistant") return std::nullopt;
  const auto& content=message.at("message").at("content");
  if (!content.is_array()) return std::nullopt;
  std::string result;bool first=true;
  for (const auto& block:content) if (block.at("type")=="text") {
    if (!first) result+='\n';
    first=false;result+=block.at("text").get<std::string>();
  }
  result=value::trim(result);
  return result.empty() ? std::nullopt : std::optional(result);
}
Json createCompactBoundaryMessage(const std::string& trigger,double preTokens,
  const std::optional<std::string>& last,const std::optional<std::string>& userContext,
  const std::optional<double>& summarized,BoundaryServices& services) {
  Json result={{"type","system"},{"subtype","compact_boundary"},{"content","Conversation compacted"},
    {"isMeta",false},{"timestamp",services.nowISO()},{"uuid",services.randomUUID()},{"level","info"},
    {"compactMetadata",{{"trigger",trigger},{"preTokens",preTokens}}}};
  if (userContext) result["compactMetadata"]["userContext"]=*userContext;
  if (summarized) result["compactMetadata"]["messagesSummarized"]=*summarized;
  if (last && !last->empty()) result["logicalParentUuid"]=*last;
  return result;
}
bool isCompactBoundaryMessage(const Json& message) {
  return value::property(message,"type")=="system" && value::property(message,"subtype")=="compact_boundary";
}
std::ptrdiff_t findLastCompactBoundaryIndex(const Json& messages) {
  for (std::ptrdiff_t i=static_cast<std::ptrdiff_t>(messages.size())-1;i>=0;--i)
    if (value::truthy(messages[i]) && isCompactBoundaryMessage(messages[i])) return i;
  return -1;
}
Json getMessagesAfterCompactBoundary(const Json& messages,bool includeSnipped,BoundaryServices& services) {
  const auto index=findLastCompactBoundaryIndex(messages);
  const auto sliced=index<0 ? messages : Json(messages.begin()+index,messages.end());
  if (!includeSnipped && services.feature("HISTORY_SNIP")) return services.projectSnippedView(sliced);
  return sliced;
}
}
