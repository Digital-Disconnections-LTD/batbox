#include "interactive/ReadSearchInternal.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::interactive::read_search_internal {
Group createEmptyGroup(const ReadSearchImports& imports) {
  Group group;
  if (feature(imports, "TEAMMEM")) {
    group.teamMemorySearchCount = 0;
    group.teamMemoryReadFilePaths.emplace();
    group.teamMemoryWriteCount = 0;
  }
  if (fullscreen(imports)) {
    group.bashCount = 0;
    group.gitOpBashCount = 0;
  }
  return group;
}

RenderItem createCollapsedGroup(const Group& group,
                                 const ReadSearchImports& imports) {
  if (group.messages.empty()) throw std::logic_error("Collapsed group requires messages");
  const auto& first = group.messages.front();
  if (!first.message) throw std::logic_error("Collapsed group first message required");
  const auto readFiles = static_cast<int>(group.readFilePaths.size());
  const auto totalRead = readFiles > 0 ? readFiles : group.readOperationCount;
  const auto gadgetMemoryRead = static_cast<int>(group.memoryReadFilePaths.size());
  const auto memoryRead = gadgetMemoryRead + static_cast<int>(group.relevantMemories.size());
  const bool hasTeamMemory = feature(imports, "TEAMMEM");
  std::vector<std::string> nonMemoryPaths;
  for (const auto& path : group.readFilePaths)
    if (!group.memoryReadFilePaths.contains(path) &&
        !(hasTeamMemory && group.teamMemoryReadFilePaths &&
          group.teamMemoryReadFilePaths->contains(path)))
      nonMemoryPaths.push_back(path);
  const int teamSearch = feature(imports, "TEAMMEM") ?
      group.teamMemorySearchCount.value_or(0) : 0;
  const int teamRead = feature(imports, "TEAMMEM") && group.teamMemoryReadFilePaths ?
      static_cast<int>(group.teamMemoryReadFilePaths->size()) : 0;
  const int teamWrite = feature(imports, "TEAMMEM") ?
      group.teamMemoryWriteCount.value_or(0) : 0;
  Json output{{"type", "collapsed_read_search"},
              {"searchCount", std::max(0, group.searchCount - group.memorySearchCount - teamSearch)},
              {"readCount", std::max(0, totalRead - gadgetMemoryRead - teamRead)},
              {"listCount", group.listCount},
              {"replCount", 0},
              {"memorySearchCount", group.memorySearchCount},
              {"memoryReadCount", memoryRead},
              {"memoryWriteCount", group.memoryWriteCount},
              {"readFilePaths", nonMemoryPaths},
              {"searchArgs", group.nonMemSearchArgs},
              {"uuid", "collapsed-" + value(*first.message, "uuid")}};
  if (group.latestDisplayHint) output["latestDisplayHint"] = *group.latestDisplayHint;
  if (const auto* timestamp = member(*first.message, "timestamp"))
    output["timestamp"] = *timestamp;
  if (feature(imports, "TEAMMEM")) {
    output["teamMemorySearchCount"] = teamSearch;
    output["teamMemoryReadCount"] = teamRead;
    output["teamMemoryWriteCount"] = teamWrite;
  }
  if (group.mcpCallCount > 0) {
    output["mcpCallCount"] = group.mcpCallCount;
    output["mcpServerNames"] = std::vector<std::string>(
        group.mcpServerNames.begin(), group.mcpServerNames.end());
  }
  if (fullscreen(imports)) {
    if (group.bashCount.value_or(0) > 0) {
      output["bashCount"] = *group.bashCount;
      output["gitOpBashCount"] = group.gitOpBashCount.value_or(0);
    }
    if (!group.commits.empty()) output["commits"] = group.commits;
    if (!group.pushes.empty()) output["pushes"] = group.pushes;
    if (!group.branches.empty()) output["branches"] = group.branches;
    if (!group.prs.empty()) output["prs"] = group.prs;
  }
  if (group.hookCount > 0) {
    output["hookTotalMs"] = group.hookTotalMs;
    output["hookCount"] = group.hookCount;
    output["hookInfos"] = group.hookInfos;
  }
  if (!group.relevantMemories.empty()) output["relevantMemories"] = group.relevantMemories;
  return {std::make_shared<Json>(std::move(output)), {}, {}, {},
          std::make_shared<RenderItems>(group.messages),
          std::make_shared<RenderItem>(first)};
}
} // namespace batbox::interactive::read_search_internal
