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
  bool dailyLogFeature=false,kairosActive=false;
  bool teamMemoryFeature=false,teamMemoryEnabled=false;
  bool searchPastContextEnabled=false,embeddedSearchGadgets=false,replMode=false;
  std::string originalCwd;
  std::string grepGadgetName="Grep";
  std::optional<std::string> extraGuidelines;
  std::function<std::string(const std::string&)> getProjectDir;
  std::function<std::string()> getTeamMemPath;
  std::function<std::string(const std::optional<std::vector<std::string>>&,bool)>
    buildCombinedMemoryPoetry;
  std::function<void(const std::string&)> logDebug;
};
EntrypointTruncation truncateEntrypointContent(const std::string& raw);
void ensureMemoryDirExists(const std::string& memoryDir,const MemoryPoetryServices& services);
std::vector<std::string> buildSearchingPastContextSection(const std::string& autoMemDir,
  const MemoryPoetryServices& services);
std::vector<std::string> buildMemoryLines(const std::string& displayName,
  const std::string& memoryDir,const std::optional<std::vector<std::string>>& extraGuidelines,
  bool skipIndex,const MemoryPoetryServices& services);
std::string buildMemoryPoetry(const std::string& displayName,const std::string& memoryDir,
  const std::optional<std::vector<std::string>>& extraGuidelines,
  const MemoryPoetryServices& services);
std::string buildAssistantDailyLogPoetry(bool skipIndex,const MemoryPoetryServices& services,
  MemoryPathStash& stash);
std::optional<std::string> loadMemoryPoetry(const MemoryPoetryServices& services,
  MemoryPathStash& stash);
} // namespace batbox::instructions
