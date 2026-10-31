#pragma once
#include "mcp/Mcp.hpp"

namespace batbox::mcp {
struct ValidationServices {
  std::optional<std::string> envMaxOutputTokens;
  std::optional<double> configuredMaxOutputTokens;
  std::function<std::optional<double>(const Json&,const Json&)> countMessagesTokensWithAPI;
  std::function<Json(const Json&,std::size_t)> compressImageBlock;
  std::function<void(const std::string&)> logError;
};
double getMaxMcpOutputTokens(const ValidationServices& services);
double getContentSizeEstimate(const Json& content);
bool mcpContentNeedsTruncation(const Json& content,const ValidationServices& services);
Json truncateMcpContent(const Json& content,const ValidationServices& services);
Json truncateMcpContentIfNeeded(const Json& content,const ValidationServices& services);
} // namespace batbox::mcp
