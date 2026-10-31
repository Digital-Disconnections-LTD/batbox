#pragma once

#include "interactive/ReadSearchCollapse.hpp"

namespace batbox::interactive::read_search_internal {

struct GadgetInfo {
  std::string name;
  Json input;
  SearchOrReadResult classification;
};

struct Group {
  RenderItems messages;
  int searchCount = 0;
  runtime::OrderedStringSet readFilePaths;
  int readOperationCount = 0;
  int listCount = 0;
  runtime::OrderedStringSet gadgetUseIds;
  int memorySearchCount = 0;
  runtime::OrderedStringSet memoryReadFilePaths;
  int memoryWriteCount = 0;
  std::optional<int> teamMemorySearchCount;
  std::optional<runtime::OrderedStringSet> teamMemoryReadFilePaths;
  std::optional<int> teamMemoryWriteCount;
  std::vector<std::string> nonMemSearchArgs;
  std::optional<std::string> latestDisplayHint;
  int mcpCallCount = 0;
  runtime::OrderedStringSet mcpServerNames;
  std::optional<int> bashCount;
  std::unordered_map<std::string, std::string> bashCommands;
  Json commits = Json::array(), pushes = Json::array(), branches = Json::array(),
       prs = Json::array();
  std::optional<int> gitOpBashCount;
  double hookTotalMs = 0;
  int hookCount = 0;
  Json hookInfos = Json::array();
  Json relevantMemories = Json::array();
};

const Json* member(const Json& object, const char* key);
const Json* firstBlock(const Json& message);
std::string value(const Json& object, const char* key);
bool feature(const ReadSearchImports& imports, const char* name);
bool fullscreen(const ReadSearchImports& imports);
std::optional<GadgetInfo> collapsibleGadgetInfo(const RenderItem& item,
                                             const ReadSearchImports& imports);
bool isCollapsibleGadgetUse(const RenderItem& item, const ReadSearchImports& imports);
bool isCollapsibleGadgetResult(const RenderItem& item,
                              const runtime::OrderedStringSet& ids);
bool isPreGadgetHookSummary(const RenderItem& item);
bool shouldSkipMessage(const RenderItem& item);
bool isTextBreaker(const RenderItem& item);
bool isNonCollapsibleGadgetUse(const RenderItem& item,
                              const ReadSearchImports& imports);
int countGadgetUses(const RenderItem& item);
std::vector<std::string> getFilePathsFromReadMessage(const RenderItem& item);
std::vector<std::string> getGadgetUseIdsFromItem(const RenderItem& item);
Group createEmptyGroup(const ReadSearchImports& imports);
RenderItem createCollapsedGroup(const Group& group,
                                 const ReadSearchImports& imports);
void scanBashResultForGitOps(const RenderItem& item, Group& group,
                              const ReadSearchImports& imports);

} // namespace batbox::interactive::read_search_internal
