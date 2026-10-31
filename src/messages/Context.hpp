#pragma once
#include "messages/Messages.hpp"
#include <optional>
namespace batbox::messages {
struct BoundaryServices {
  std::function<std::string()> randomUUID;
  std::function<std::string()> nowISO;
  std::function<bool(const std::string&)> feature;
  std::function<Json(const Json&)> projectSnippedView;
};
Json getLastAssistantMessage(const Json& messages);
std::optional<std::string> getAssistantMessageText(const Json& message);
Json createCompactBoundaryMessage(const std::string& trigger,double preTokens,
  const std::optional<std::string>& lastUuid,const std::optional<std::string>& userContext,
  const std::optional<double>& summarized,BoundaryServices& services);
bool isCompactBoundaryMessage(const Json& message);
std::ptrdiff_t findLastCompactBoundaryIndex(const Json& messages);
Json getMessagesAfterCompactBoundary(const Json& messages,bool includeSnipped,BoundaryServices& services);
}
