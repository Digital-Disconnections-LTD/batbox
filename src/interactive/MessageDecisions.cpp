#include "interactive/MessageDecisions.hpp"
#include <stdexcept>

namespace batbox::interactive {
namespace {
const Json* member(const Json& value, const char* key) {
  if (!value.is_object()) return nullptr;
  const auto found = value.find(key);
  return found == value.end() ? nullptr : &*found;
}
const Json* firstBlock(const Json& message) {
  const auto* inner = member(message, "message");
  const auto* content = inner ? member(*inner, "content") : nullptr;
  return content && content->is_array() && !content->empty() ? &(*content)[0] : nullptr;
}
std::string string(const Json& value, const char* key) {
  const auto* field = member(value, key);
  return field && field->is_string() ? field->get<std::string>() : "";
}
int count(const std::unordered_map<std::string, std::unordered_map<std::string, int>>& map,
          const std::string& id, const std::string& event) {
  const auto found = map.find(id);
  if (found == map.end()) return 0;
  const auto nested = found->second.find(event);
  return nested == found->second.end() ? 0 : nested->second;
}
std::vector<std::string> getGadgetUseIdsFromMessage(const Json& message) {
  std::vector<std::string> ids;
  if (string(message, "type") == "assistant") {
    const auto* block = firstBlock(message);
    if (block && string(*block, "type") == "tool_use") ids.push_back(string(*block, "id"));
  } else if (string(message, "type") == "grouped_tool_use") {
    const auto* messages = member(message, "messages");
    if (messages && messages->is_array()) for (const auto& nested : *messages) {
      const auto* block = firstBlock(nested);
      if (block && string(*block, "type") == "tool_use") {
        const auto id = string(*block, "id");
        if (!id.empty()) ids.push_back(id);
      }
    }
  }
  return ids;
}
bool collapsible(const MessageDecisionImports& imports, const std::string& name,
                 const Json& input) {
  if (!imports.isCollapsibleGadget)
    throw std::logic_error("Gadget grouping import required");
  return imports.isCollapsibleGadget(name, input);
}
} // namespace

std::vector<std::string> getGadgetUseIdsFromCollapsedGroup(const Json& message) {
  std::vector<std::string> ids;
  const auto* messages = member(message, "messages");
  if (!messages || !messages->is_array()) return ids;
  for (const auto& nested : *messages) {
    auto next = getGadgetUseIdsFromMessage(nested);
    ids.insert(ids.end(), next.begin(), next.end());
  }
  return ids;
}

bool hasUnresolvedHooksFromLookup(const std::string& gadgetUseID,
                                  const std::string& hookEvent,
                                  const MessageDecisionLookups& lookups) {
  return count(lookups.inProgressHookCounts, gadgetUseID, hookEvent) >
      count(lookups.resolvedHookCounts, gadgetUseID, hookEvent);
}

bool hasContentAfterIndex(const MessageRefs& messages, std::size_t index,
                          const GadgetIDs& streamingGadgetUseIDs,
                          const MessageDecisionImports& imports) {
  for (std::size_t i = index + 1; i < messages.size(); ++i) {
    if (!messages[i]) continue;
    const auto& message = *messages[i];
    const auto type = string(message, "type");
    if (type == "assistant") {
      const auto* block = firstBlock(message);
      if (block && (string(*block, "type") == "thinking" ||
                    string(*block, "type") == "redacted_thinking")) continue;
      if (block && string(*block, "type") == "tool_use") {
        const auto* input = member(*block, "input");
        if (collapsible(imports, string(*block, "name"), input ? *input : Json(nullptr))) continue;
        if (streamingGadgetUseIDs.contains(string(*block, "id"))) continue;
      }
      return true;
    }
    if (type == "system" || type == "attachment") continue;
    if (type == "user") {
      const auto* block = firstBlock(message);
      if (block && string(*block, "type") == "tool_result") continue;
    }
    if (type == "grouped_tool_use") {
      const auto* nested = member(message, "messages");
      const auto* first = nested && nested->is_array() && !nested->empty() ? &(*nested)[0] : nullptr;
      const auto* block = first ? firstBlock(*first) : nullptr;
      const auto* input = block ? member(*block, "input") : nullptr;
      if (collapsible(imports, string(message, "toolName"), input ? *input : Json(nullptr))) continue;
    }
    return true;
  }
  return false;
}

bool isMessageStreaming(const Json& message, const GadgetIDs& streamingGadgetUseIDs) {
  const auto type = string(message, "type");
  if (type == "grouped_tool_use") {
    const auto* messages = member(message, "messages");
    if (messages && messages->is_array()) for (const auto& nested : *messages) {
      const auto* block = firstBlock(nested);
      if (block && string(*block, "type") == "tool_use" &&
          streamingGadgetUseIDs.contains(string(*block, "id"))) return true;
    }
    return false;
  }
  if (type == "collapsed_read_search") {
    for (const auto& id : getGadgetUseIdsFromCollapsedGroup(message))
      if (streamingGadgetUseIDs.contains(id)) return true;
    return false;
  }
  const auto id = getGadgetUseID(message);
  return id && !id->empty() && streamingGadgetUseIDs.contains(*id);
}

bool allGadgetsResolved(const Json& message, const GadgetIDs& resolvedGadgetUseIDs) {
  const auto type = string(message, "type");
  if (type == "grouped_tool_use") {
    const auto* messages = member(message, "messages");
    if (messages && messages->is_array()) for (const auto& nested : *messages) {
      const auto* block = firstBlock(nested);
      if (!block || string(*block, "type") != "tool_use" ||
          !resolvedGadgetUseIDs.contains(string(*block, "id"))) return false;
    }
    return true;
  }
  if (type == "collapsed_read_search") {
    for (const auto& id : getGadgetUseIdsFromCollapsedGroup(message))
      if (!resolvedGadgetUseIDs.contains(id)) return false;
    return true;
  }
  if (type == "assistant") {
    const auto* block = firstBlock(message);
    if (block && string(*block, "type") == "server_tool_use")
      return resolvedGadgetUseIDs.contains(string(*block, "id"));
  }
  const auto id = getGadgetUseID(message);
  return !id || id->empty() || resolvedGadgetUseIDs.contains(*id);
}

bool shouldRenderStatically(const Json& message,
                            const GadgetIDs& streamingGadgetUseIDs,
                            const GadgetIDs& inProgressGadgetUseIDs,
                            const GadgetIDs& siblingGadgetUseIDs,
                            const std::string& screen,
                            const MessageDecisionLookups& lookups) {
  if (screen == "transcript") return true;
  const auto type = string(message, "type");
  if (type == "attachment" || type == "user" || type == "assistant") {
    if (type == "assistant") {
      const auto* block = firstBlock(message);
      if (block && string(*block, "type") == "server_tool_use")
        return lookups.resolvedGadgetUseIDs.contains(string(*block, "id"));
    }
    const auto id = getGadgetUseID(message);
    if (!id || id->empty()) return true;
    if (streamingGadgetUseIDs.contains(*id)) return false;
    if (inProgressGadgetUseIDs.contains(*id)) return false;
    if (hasUnresolvedHooksFromLookup(*id, "PostToolUse", lookups)) return false;
    for (const auto& sibling : siblingGadgetUseIDs)
      if (!lookups.resolvedGadgetUseIDs.contains(sibling)) return false;
    return true;
  }
  if (type == "system") return string(message, "subtype") != "api_error";
  if (type == "grouped_tool_use") {
    const auto* messages = member(message, "messages");
    if (messages && messages->is_array()) for (const auto& nested : *messages) {
      const auto* block = firstBlock(nested);
      if (!block || string(*block, "type") != "tool_use" ||
          !lookups.resolvedGadgetUseIDs.contains(string(*block, "id"))) return false;
    }
    return true;
  }
  if (type == "collapsed_read_search") return false;
  return false;
}

std::string expandKey(const Json& message) {
  const auto type = string(message, "type");
  if (type == "assistant" || type == "user") {
    const auto id = getGadgetUseID(message);
    if (id) return *id;
  }
  return string(message, "uuid");
}

} // namespace batbox::interactive
