#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::instructions {
struct ParsedMarkdown {Json frontmatter=Json::object();std::string content;};
ParsedMarkdown parseFrontmatter(const std::string& markdown,
  const std::optional<std::string>& sourcePath=std::nullopt,
  const std::function<void(const std::string&)>& logDebug={});
std::vector<std::string> splitPathInFrontmatter(const Json& input);
std::optional<std::string> coerceDescriptionToString(const Json& value);
bool parseBooleanFrontmatter(const Json& value);
std::optional<std::string> parseShellFrontmatter(const Json& value,
  const std::string& source,const std::function<void(const std::string&)>& logDebug={});
std::string extractDescriptionFromMarkdown(const std::string& content,
  const std::string& defaultDescription="Custom item");
} // namespace batbox::instructions
