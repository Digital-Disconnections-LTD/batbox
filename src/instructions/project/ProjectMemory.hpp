#pragma once
#include "filesystem/FileOps.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::instructions::project {
struct MemoryFileInfo {
  std::string path,type,content;
  std::optional<std::string> parent;
  std::optional<std::vector<std::string>> globs;
  bool contentDiffersFromDisk=false;
  std::optional<std::string> rawContent;
};
struct ParsedMemoryContent {
  std::optional<MemoryFileInfo> info;
  std::vector<std::string> includePaths;
};
struct StrippedComments {std::string content;bool stripped=false;};
struct FrontmatterPaths {
  std::string content;
  std::optional<std::vector<std::string>> paths;
};
struct ParseServices {
  filesystem::PathServices paths;
  std::function<void(const std::string&)> logDebug;
};
FrontmatterPaths parseFrontmatterPaths(const std::string& rawContent);
StrippedComments stripHtmlCommentsFromTokens(const Json& tokens);
StrippedComments stripHtmlComments(const std::string& content);
std::vector<std::string> extractIncludePathsFromTokens(const Json& tokens,
  const std::string& basePath,const ParseServices& services);
ParsedMemoryContent parseMemoryFileContent(const std::string& rawContent,
  const std::string& filePath,const std::string& type,
  const std::optional<std::string>& includeBasePath,
  const ParseServices& services);
Json memoryFileToJson(const MemoryFileInfo& info);
}
