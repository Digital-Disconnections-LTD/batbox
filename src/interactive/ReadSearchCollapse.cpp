#include "interactive/ReadSearchInternal.hpp"
#include <stdexcept>

namespace batbox::interactive {
using namespace read_search_internal;
namespace {
template<class T> void need(const T& callback, const char* name) {
  if (!callback) throw std::logic_error(std::string(name) + " import required");
}
std::string inputString(const Json& input, const char* key) {
  return value(input, key);
}
void appendArray(Json& into, const Json& from) {
  if (!from.is_array()) throw std::runtime_error("Array required");
  for (const auto& item : from) into.push_back(item);
}
} // namespace

RenderItemsRef collapseReadSearchGroups(const RenderItemsRef& messages,
                                         const ReadSearchImports& imports) {
  if (!messages) throw std::invalid_argument("Message list required");
  auto result = std::make_shared<RenderItems>();
  auto current = createEmptyGroup(imports);
  RenderItems deferredSkippable;
  auto flush = [&] {
    if (current.messages.empty()) return;
    result->push_back(createCollapsedGroup(current, imports));
    result->insert(result->end(), deferredSkippable.begin(), deferredSkippable.end());
    deferredSkippable.clear();
    current = createEmptyGroup(imports);
  };
  for (const auto& item : *messages) {
    if (isCollapsibleGadgetUse(item, imports)) {
      const auto info = collapsibleGadgetInfo(item, imports);
      if (!info) throw std::logic_error("Collapsible Gadget info missing");
      const auto count = countGadgetUses(item);
      if (info->classification.isMemoryWrite) {
        if (feature(imports, "TEAMMEM")) {
          need(imports.isTeamMemoryWriteOrEdit, "isTeamMemoryWriteOrEdit");
          if (imports.isTeamMemoryWriteOrEdit(info->name, info->input))
            current.teamMemoryWriteCount = current.teamMemoryWriteCount.value_or(0) + count;
          else current.memoryWriteCount += count;
        } else current.memoryWriteCount += count;
      } else if (info->classification.isAbsorbedSilently) {
        // The message stays in the group without contributing to a count.
      } else if (info->classification.mcpServerName &&
                 !info->classification.mcpServerName->empty()) {
        current.mcpCallCount += count;
        current.mcpServerNames.insert(*info->classification.mcpServerName);
        const auto query = inputString(info->input, "query");
        if (!query.empty()) current.latestDisplayHint = "\"" + query + "\"";
      } else if (fullscreen(imports) &&
                 info->classification.isBash.value_or(false)) {
        current.bashCount = current.bashCount.value_or(0) + count;
        const auto command = inputString(info->input, "command");
        if (!command.empty()) {
          need(imports.extractBashCommentLabel, "extractBashCommentLabel");
          const auto comment = imports.extractBashCommentLabel(command);
          if (comment) current.latestDisplayHint = *comment;
          else current.latestDisplayHint = commandAsHint(command);
          for (const auto& id : getGadgetUseIdsFromItem(item))
            current.bashCommands[id] = command;
        }
      } else if (info->classification.isList) {
        current.listCount += count;
        const auto command = inputString(info->input, "command");
        if (!command.empty()) {
          current.latestDisplayHint = commandAsHint(command);
        }
      } else if (info->classification.isSearch) {
        current.searchCount += count;
        bool teamMemory = false;
        if (feature(imports, "TEAMMEM")) {
          need(imports.isTeamMemorySearch, "isTeamMemorySearch");
          teamMemory = imports.isTeamMemorySearch(info->input);
        }
        if (teamMemory)
          current.teamMemorySearchCount = current.teamMemorySearchCount.value_or(0) + count;
        else if (isMemorySearch(info->input, imports.classification))
          current.memorySearchCount += count;
        else {
          const auto pattern = inputString(info->input, "pattern");
          if (!pattern.empty()) {
            current.nonMemSearchArgs.push_back(pattern);
            current.latestDisplayHint = "\"" + pattern + "\"";
          }
        }
      } else {
        const auto paths = getFilePathsFromReadMessage(item);
        for (const auto& path : paths) {
          current.readFilePaths.insert(path);
          bool teamMemory = false;
          if (feature(imports, "TEAMMEM")) {
            need(imports.isTeamMemFile, "isTeamMemFile");
            teamMemory = imports.isTeamMemFile(path);
          }
          if (teamMemory) current.teamMemoryReadFilePaths->insert(path);
          else {
            need(imports.classification.isAutoManagedMemoryFile,
                 "isAutoManagedMemoryFile");
            if (imports.classification.isAutoManagedMemoryFile(path))
              current.memoryReadFilePaths.insert(path);
            else {
              need(imports.getDisplayPath, "getDisplayPath");
              current.latestDisplayHint = imports.getDisplayPath(path);
            }
          }
        }
        if (paths.empty()) {
          current.readOperationCount += count;
          const auto command = inputString(info->input, "command");
          if (!command.empty()) {
            current.latestDisplayHint = commandAsHint(command);
          }
        }
      }
      for (const auto& id : getGadgetUseIdsFromItem(item))
        current.gadgetUseIds.insert(id);
      current.messages.push_back(item);
    } else if (isCollapsibleGadgetResult(item, current.gadgetUseIds)) {
      current.messages.push_back(item);
      if (fullscreen(imports) && !current.bashCommands.empty())
        scanBashResultForGitOps(item, current, imports);
    } else if (!current.messages.empty() && isPreGadgetHookSummary(item)) {
      const auto& message = *item.message;
      current.hookCount += message.at("hookCount").get<int>();
      const auto* duration = member(message, "totalDurationMs");
      if (duration && !duration->is_null())
        current.hookTotalMs += duration->get<double>();
      else {
        for (const auto& hook : message.at("hookInfos")) {
          const auto* value = member(hook, "durationMs");
          if (value && !value->is_null()) current.hookTotalMs += value->get<double>();
        }
      }
      appendArray(current.hookInfos, message.at("hookInfos"));
    } else if (!current.messages.empty() && item.message &&
               value(*item.message, "type") == "attachment" &&
               member(*item.message, "attachment") &&
               value(*member(*item.message, "attachment"), "type") == "relevant_memories") {
      appendArray(current.relevantMemories,
                  member(*item.message, "attachment")->at("memories"));
    } else if (shouldSkipMessage(item)) {
      const auto* attachment = item.message ? member(*item.message, "attachment") : nullptr;
      const bool nested = item.message && value(*item.message, "type") == "attachment" &&
          attachment && value(*attachment, "type") == "nested_memory";
      if (!current.messages.empty() && !nested)
        deferredSkippable.push_back(item);
      else result->push_back(item);
    } else if (isTextBreaker(item)) {
      flush(); result->push_back(item);
    } else if (isNonCollapsibleGadgetUse(item, imports)) {
      flush(); result->push_back(item);
    } else {
      flush(); result->push_back(item);
    }
  }
  flush();
  return result;
}
} // namespace batbox::interactive
