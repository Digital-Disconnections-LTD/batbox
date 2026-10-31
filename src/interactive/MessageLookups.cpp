#include "interactive/MessageLookups.hpp"
#include <stdexcept>

namespace batbox::interactive {
namespace {
const Json* member(const Json& object, const char* key) {
  if (!object.is_object()) return nullptr;
  const auto found = object.find(key);
  return found == object.end() ? nullptr : &*found;
}
const Json* content(const Json& message) {
  const auto* inner = member(message, "message");
  return inner ? member(*inner, "content") : nullptr;
}
std::string value(const Json& object, const char* key) {
  const auto* field = member(object, key);
  return field && field->is_string() ? field->get<std::string>() : "";
}
} // namespace

const Json& GadgetBlockRef::value() const {
  if (!owner) throw std::logic_error("Gadget block owner missing");
  const auto* blocks = content(*owner);
  if (!blocks || !blocks->is_array() || index >= blocks->size())
    throw std::out_of_range("Gadget block no longer present");
  return (*blocks)[index];
}

MessageLookups buildMessageLookups(const MessageRefs& normalizedMessages,
                                   const MessageRefs& messages) {
  MessageLookups result;
  result.normalizedMessageCount = normalizedMessages.size();
  std::unordered_map<std::string, std::shared_ptr<runtime::OrderedStringSet>> idsByMessage;
  std::unordered_map<std::string, std::string> idToMessage;
  for (const auto& ref : messages) {
    if (!ref || value(*ref, "type") != "assistant") continue;
    const auto* inner = member(*ref, "message");
    const auto messageId = inner ? value(*inner, "id") : "";
    auto& ids = idsByMessage[messageId];
    if (!ids) ids = std::make_shared<runtime::OrderedStringSet>();
    const auto* blocks = content(*ref);
    if (!blocks || !blocks->is_array()) continue;
    for (std::size_t index = 0; index < blocks->size(); ++index) {
      const auto& block = (*blocks)[index];
      if (value(block, "type") != "tool_use") continue;
      const auto id = value(block, "id");
      ids->insert(id);
      idToMessage[id] = messageId;
      result.gadgetUseByGadgetUseID[id] = GadgetBlockRef{ref, index};
    }
  }
  for (const auto& [id, messageId] : idToMessage)
    result.siblingGadgetUseIDs[id] = idsByMessage.at(messageId);

  std::unordered_map<std::string,
      std::unordered_map<std::string, runtime::OrderedStringSet>> resolvedHookNames;
  for (const auto& ref : normalizedMessages) {
    if (!ref) continue;
    const auto type = value(*ref, "type");
    if (type == "progress") {
      const auto id = value(*ref, "parentToolUseID");
      result.progressMessagesByGadgetUseID[id].push_back(ref);
      const auto* data = member(*ref, "data");
      if (data && value(*data, "type") == "hook_progress")
        ++result.inProgressHookCounts[id][value(*data, "hookEvent")];
    }
    if (type == "user" || type == "assistant") {
      const auto* blocks = content(*ref);
      if (blocks && blocks->is_array()) for (const auto& block : *blocks) {
        if (type == "user" && value(block, "type") == "tool_result") {
          const auto id = value(block, "tool_use_id");
          result.gadgetResultByGadgetUseID[id] = ref;
          result.resolvedGadgetUseIDs.insert(id);
          if (value::truthy(value::property(block, "is_error"))) result.erroredGadgetUseIDs.insert(id);
        }
        if (type == "assistant") {
          if (const auto* id = member(block, "tool_use_id"); id && id->is_string())
            result.resolvedGadgetUseIDs.insert(id->get<std::string>());
          if (value(block, "type") == "advisor_tool_result") {
            const auto* nested = member(block, "content");
            if (nested && value(*nested, "type") == "advisor_tool_result_error")
              result.erroredGadgetUseIDs.insert(value(block, "tool_use_id"));
          }
        }
      }
    }
    if (isHookAttachmentMessage(*ref)) {
      const auto& attachment = *member(*ref, "attachment");
      const auto* name = member(attachment, "hookName");
      if (name) resolvedHookNames[value(attachment, "toolUseID")]
          [value(attachment, "hookEvent")].insert(name->is_string() ?
          name->get<std::string>() : name->dump());
    }
  }
  for (const auto& [id, events] : resolvedHookNames)
    for (const auto& [event, names] : events)
      result.resolvedHookCounts[id][event] = static_cast<int>(names.size());

  std::string lastAssistantMessageId;
  bool hasLastAssistant = false;
  if (!messages.empty() && messages.back() &&
      value(*messages.back(), "type") == "assistant") {
    const auto* inner = member(*messages.back(), "message");
    lastAssistantMessageId = inner ? value(*inner, "id") : "";
    hasLastAssistant = true;
  }
  for (const auto& ref : normalizedMessages) {
    if (!ref || value(*ref, "type") != "assistant") continue;
    const auto* inner = member(*ref, "message");
    if (hasLastAssistant && inner && value(*inner, "id") == lastAssistantMessageId) continue;
    const auto* blocks = content(*ref);
    if (!blocks || !blocks->is_array()) continue;
    for (const auto& block : *blocks) {
      const auto type = value(block, "type");
      if (type != "server_tool_use" && type != "mcp_tool_use") continue;
      const auto id = value(block, "id");
      if (!result.resolvedGadgetUseIDs.contains(id)) {
        result.resolvedGadgetUseIDs.insert(id);
        result.erroredGadgetUseIDs.insert(id);
      }
    }
  }
  return result;
}
} // namespace batbox::interactive
