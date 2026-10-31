#include "interactive/MessageOrder.hpp"
#include <unordered_map>
#include <unordered_set>

namespace batbox::interactive {
namespace {
const Json* member(const Json& object, const char* key) {
  if (!object.is_object()) return nullptr;
  const auto found = object.find(key);
  return found == object.end() ? nullptr : &*found;
}
const Json* firstBlock(const Json& message) {
  const auto* inner = member(message, "message");
  const auto* content = inner ? member(*inner, "content") : nullptr;
  return content && content->is_array() && !content->empty() ?
      &(*content)[0] : nullptr;
}
std::string value(const Json& object, const char* key) {
  const auto* found = member(object, key);
  return found && found->is_string() ? found->get<std::string>() : "";
}
std::string type(const Json& object) { return value(object, "type"); }
bool isApiError(const Json& message) {
  return type(message) == "system" && value(message, "subtype") == "api_error";
}
struct GadgetUseGroup {
  MessageRef gadgetUse;
  MessageRefs preHooks;
  MessageRef gadgetResult;
  MessageRefs postHooks;
};
} // namespace

bool isGadgetUseRequestMessage(const Json& message) {
  if (type(message) != "assistant") return false;
  const auto* inner = member(message, "message");
  const auto* content = inner ? member(*inner, "content") : nullptr;
  if (!content || !content->is_array()) return false;
  for (const auto& block : *content)
    if (type(block) == "tool_use") return true;
  return false;
}

bool isGadgetUseResultMessage(const Json& message) {
  if (type(message) != "user") return false;
  const auto* block = firstBlock(message);
  return (block && type(*block) == "tool_result") ||
      value::truthy(value::property(message, "toolUseResult"));
}

bool isHookAttachmentMessage(const Json& message) {
  if (type(message) != "attachment") return false;
  const auto* attachment = member(message, "attachment");
  if (!attachment) return false;
  const auto kind = type(*attachment);
  return kind == "hook_blocking_error" || kind == "hook_cancelled" ||
      kind == "hook_error_during_execution" ||
      kind == "hook_non_blocking_error" || kind == "hook_success" ||
      kind == "hook_system_message" ||
      kind == "hook_additional_context" ||
      kind == "hook_stopped_continuation";
}

std::optional<std::string> getGadgetUseID(const Json& message) {
  const auto messageType = type(message);
  if (messageType == "attachment") {
    if (isHookAttachmentMessage(message))
      return value(*member(message, "attachment"), "toolUseID");
    return {};
  }
  if (messageType == "assistant") {
    const auto* block = firstBlock(message);
    if (!block || type(*block) != "tool_use") return {};
    return value(*block, "id");
  }
  if (messageType == "user") {
    const auto source = value(message, "sourceToolUseID");
    if (!source.empty()) return source;
    const auto* block = firstBlock(message);
    if (!block || type(*block) != "tool_result") return {};
    return value(*block, "tool_use_id");
  }
  if (messageType == "progress") return value(message, "toolUseID");
  if (messageType == "system" && value(message, "subtype") == "informational") {
    const auto* id = member(message, "toolUseID");
    if (id && id->is_string()) return id->get<std::string>();
  }
  return {};
}

runtime::OrderedStringSet getGadgetUseIDs(const MessageRefs& messages) {
  runtime::OrderedStringSet result;
  for (const auto& message : messages) {
    if (!message || type(*message) != "assistant") continue;
    const auto* block = firstBlock(*message);
    if (block && type(*block) == "tool_use")
      result.insert(value(*block, "id"));
  }
  return result;
}

MessageRefs reorderMessagesInUI(const MessageRefs& messages,
                                 const MessageRefs& synthetic) {
  std::unordered_map<std::string, GadgetUseGroup> groups;
  for (const auto& message : messages) {
    if (!message) continue;
    if (isGadgetUseRequestMessage(*message)) {
      const auto* block = firstBlock(*message);
      const auto id = block ? value(*block, "id") : "";
      if (!id.empty()) groups[id].gadgetUse = message;
      continue;
    }
    if (isHookAttachmentMessage(*message)) {
      const auto& attachment = *member(*message, "attachment");
      if (value(attachment, "hookEvent") == "PreToolUse") {
        groups[value(attachment, "toolUseID")].preHooks.push_back(message);
        continue;
      }
    }
    if (type(*message) == "user") {
      const auto* block = firstBlock(*message);
      if (block && type(*block) == "tool_result") {
        groups[value(*block, "tool_use_id")].gadgetResult = message;
        continue;
      }
    }
    if (isHookAttachmentMessage(*message)) {
      const auto& attachment = *member(*message, "attachment");
      if (value(attachment, "hookEvent") == "PostToolUse") {
        groups[value(attachment, "toolUseID")].postHooks.push_back(message);
        continue;
      }
    }
  }

  MessageRefs result;
  std::unordered_set<std::string> processed;
  for (const auto& message : messages) {
    if (!message) continue;
    if (isGadgetUseRequestMessage(*message)) {
      const auto* block = firstBlock(*message);
      const auto id = block ? value(*block, "id") : "";
      if (!id.empty() && processed.insert(id).second) {
        const auto group = groups.find(id);
        if (group != groups.end() && group->second.gadgetUse) {
          result.push_back(group->second.gadgetUse);
          result.insert(result.end(), group->second.preHooks.begin(),
                        group->second.preHooks.end());
          if (group->second.gadgetResult) result.push_back(group->second.gadgetResult);
          result.insert(result.end(), group->second.postHooks.begin(),
                        group->second.postHooks.end());
        }
      }
      continue;
    }
    if (isHookAttachmentMessage(*message)) {
      const auto& attachment = *member(*message, "attachment");
      const auto event = value(attachment, "hookEvent");
      if (event == "PreToolUse" || event == "PostToolUse") continue;
    }
    if (type(*message) == "user") {
      const auto* block = firstBlock(*message);
      if (block && type(*block) == "tool_result") continue;
    }
    if (isApiError(*message)) {
      if (!result.empty() && result.back() && isApiError(*result.back()))
        result.back() = message;
      else result.push_back(message);
      continue;
    }
    result.push_back(message);
  }
  result.insert(result.end(), synthetic.begin(), synthetic.end());
  const auto last = result.empty() ? MessageRef{} : result.back();
  MessageRefs filtered;
  for (const auto& message : result)
    if (message && (!isApiError(*message) || message == last))
      filtered.push_back(message);
  return filtered;
}
} // namespace batbox::interactive
