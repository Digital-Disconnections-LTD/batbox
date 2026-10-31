#include "interactive/ReadSearchInternal.hpp"
#include <stdexcept>

namespace batbox::interactive::read_search_internal {
const Json* member(const Json& object, const char* key) {
  if (!object.is_object()) return nullptr;
  const auto found = object.find(key);
  return found == object.end() ? nullptr : &*found;
}
const Json* firstBlock(const Json& message) {
  const auto* inner = member(message, "message");
  const auto* content = inner ? member(*inner, "content") : nullptr;
  return content && content->is_array() && !content->empty() ? &(*content)[0] : nullptr;
}
std::string value(const Json& object, const char* key) {
  const auto* found = member(object, key);
  return found && found->is_string() ? found->get<std::string>() : "";
}
bool feature(const ReadSearchImports& imports, const char* name) {
  if (!imports.classification.feature) throw std::logic_error("feature import required");
  return imports.classification.feature(name);
}
bool fullscreen(const ReadSearchImports& imports) {
  if (!imports.classification.isFullscreen)
    throw std::logic_error("isFullscreen import required");
  return imports.classification.isFullscreen();
}
namespace {
const Json* firstGadgetBlock(const RenderItem& item) {
  if (!item.message) return nullptr;
  const auto type = value(*item.message, "type");
  if (type == "assistant") return firstBlock(*item.message);
  if (type == "grouped_tool_use") {
    if (!item.members.empty()) return firstBlock(*item.members.front());
    const auto* messages = member(*item.message, "messages");
    return messages && messages->is_array() && !messages->empty() ?
        firstBlock((*messages)[0]) : nullptr;
  }
  return nullptr;
}
} // namespace

std::optional<GadgetInfo> collapsibleGadgetInfo(const RenderItem& item,
                                             const ReadSearchImports& imports) {
  if (!item.message) return {};
  const auto type = value(*item.message, "type");
  if (type != "assistant" && type != "grouped_tool_use") return {};
  const auto* block = firstGadgetBlock(item);
  if (!block || value(*block, "type") != "tool_use") return {};
  const auto name = type == "grouped_tool_use" ?
      value(*item.message, "toolName") : value(*block, "name");
  const auto* input = member(*block, "input");
  const Json inputValue = input ? *input : Json(nullptr);
  const Json surrogate{{"type", "tool_use"}, {"name", name}, {"input", inputValue}};
  const auto info = getSearchOrReadFromContent(&surrogate,
      imports.gadgets, imports.classification);
  if (!info) return {};
  return GadgetInfo{name, inputValue, *info};
}

bool isCollapsibleGadgetUse(const RenderItem& item, const ReadSearchImports& imports) {
  if (!item.message) return false;
  const auto type = value(*item.message, "type");
  if (type != "assistant" && type != "grouped_tool_use") return false;
  const auto* block = firstGadgetBlock(item);
  if (!block || value(*block, "type") != "tool_use") return false;
  const auto name = type == "grouped_tool_use" ?
      value(*item.message, "toolName") : value(*block, "name");
  const auto* input = member(*block, "input");
  return getGadgetSearchOrReadInfo(name, input ? *input : Json(nullptr),
      imports.gadgets, imports.classification).isCollapsible;
}

bool isCollapsibleGadgetResult(const RenderItem& item,
                              const runtime::OrderedStringSet& ids) {
  if (!item.message || value(*item.message, "type") != "user") return false;
  const auto* inner = member(*item.message, "message");
  const auto* content = inner ? member(*inner, "content") : nullptr;
  if (!content || !content->is_array()) return false;
  bool hasResult = false;
  for (const auto& block : *content) {
    if (value(block, "type") != "tool_result") continue;
    hasResult = true;
    if (!ids.contains(value(block, "tool_use_id"))) return false;
  }
  return hasResult;
}

bool isPreGadgetHookSummary(const RenderItem& item) {
  return item.message && value(*item.message, "type") == "system" &&
      value(*item.message, "subtype") == "stop_hook_summary" &&
      value(*item.message, "hookLabel") == "PreToolUse";
}

bool shouldSkipMessage(const RenderItem& item) {
  if (!item.message) return false;
  const auto type = value(*item.message, "type");
  if (type == "assistant") {
    const auto* block = firstBlock(*item.message);
    if (block && (value(*block, "type") == "thinking" ||
                  value(*block, "type") == "redacted_thinking")) return true;
  }
  return type == "attachment" || type == "system";
}

bool isTextBreaker(const RenderItem& item) {
  if (!item.message || value(*item.message, "type") != "assistant") return false;
  const auto* block = firstBlock(*item.message);
  return block && value(*block, "type") == "text" &&
      !value::trim(value(*block, "text")).empty();
}

bool isNonCollapsibleGadgetUse(const RenderItem& item,
                              const ReadSearchImports& imports) {
  if (!item.message) return false;
  const auto type = value(*item.message, "type");
  if (type != "assistant" && type != "grouped_tool_use") return false;
  const auto* block = firstGadgetBlock(item);
  if (!block || value(*block, "type") != "tool_use") return false;
  const auto name = type == "grouped_tool_use" ?
      value(*item.message, "toolName") : value(*block, "name");
  const auto* input = member(*block, "input");
  return !getGadgetSearchOrReadInfo(name, input ? *input : Json(nullptr),
      imports.gadgets, imports.classification).isCollapsible;
}

int countGadgetUses(const RenderItem& item) {
  return item.message && value(*item.message, "type") == "grouped_tool_use" ?
      static_cast<int>(item.members.size()) : 1;
}

std::vector<std::string> getFilePathsFromReadMessage(const RenderItem& item) {
  std::vector<std::string> paths;
  if (!item.message) return paths;
  const auto kind = value(*item.message, "type");
  auto take = [&paths](const Json& message) {
    const auto* block = firstBlock(message);
    if (!block || value(*block, "type") != "tool_use") return;
    const auto* input = member(*block, "input");
    const auto path = input ? value(*input, "file_path") : "";
    if (!path.empty()) paths.push_back(path);
  };
  if (kind == "assistant") take(*item.message);
  else if (kind == "grouped_tool_use")
    for (const auto& message : item.members) take(*message);
  return paths;
}

std::vector<std::string> getGadgetUseIdsFromItem(const RenderItem& item) {
  std::vector<std::string> ids;
  if (!item.message) return ids;
  const auto type = value(*item.message, "type");
  if (type == "assistant") {
    const auto* block = firstBlock(*item.message);
    if (block && value(*block, "type") == "tool_use")
      ids.push_back(value(*block, "id"));
  } else if (type == "grouped_tool_use") {
    for (const auto& message : item.members) {
      const auto* block = firstBlock(*message);
      if (block && value(*block, "type") == "tool_use" &&
          !value(*block, "id").empty()) ids.push_back(value(*block, "id"));
    }
  }
  return ids;
}
} // namespace batbox::interactive::read_search_internal
