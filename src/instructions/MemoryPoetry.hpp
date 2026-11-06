#pragma once
#include "instructions/MemoryPath.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::instructions {
struct EntrypointTruncation {
  std::string content;
  std::size_t lineCount=0,byteCount=0;
  bool wasLineTruncated=false,wasByteTruncated=false;
};
struct MemoryPoetryServices {
  MemoryPathServices paths;
  bool skipIndex=false;
  std::optional<std::string> extraGuidelines;
  std::function<void(const std::string&)> logDebug;
};
EntrypointTruncation truncateEntrypointContent(const std::string& raw);
void ensureMemoryDirExists(const std::string& memoryDir,const MemoryPoetryServices& services);
std::vector<std::string> buildMemoryLines(const std::string& displayName,
  const std::string& memoryDir,const std::optional<std::vector<std::string>>& extraGuidelines,
  bool skipIndex);
std::string buildMemoryPoetry(const std::string& displayName,const std::string& memoryDir,
  const std::optional<std::vector<std::string>>& extraGuidelines);
std::optional<std::string> loadMemoryPoetry(const MemoryPoetryServices& services,
  MemoryPathStash& stash);
} // namespace batbox::instructions
