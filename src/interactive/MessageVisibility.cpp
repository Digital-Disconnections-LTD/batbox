#include "interactive/MessageVisibility.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
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
bool truth(const Json& object, const char* key) {
  return value::truthy(value::property(object, key));
}
double mathMin(double a, double b) {
  if (std::isnan(a) || std::isnan(b)) return std::numeric_limits<double>::quiet_NaN();
  return std::min(a, b);
}
double mathMax(double a, double b) {
  if (std::isnan(a) || std::isnan(b)) return std::numeric_limits<double>::quiet_NaN();
  return std::max(a, b);
}
} // namespace

bool isNotEmptyMessage(const Json& message, const VisibilityImports& imports) {
  const auto type = value(message, "type");
  if (type == "progress" || type == "attachment" || type == "system") return true;
  const auto* inner = member(message, "message");
  const auto* content = inner ? member(*inner, "content") : nullptr;
  if (!content) throw std::runtime_error("Message content required");
  if (content->is_string())
    return !value::trim(content->get_ref<const std::string&>()).empty();
  if (!content->is_array()) throw std::runtime_error("Message content must be text or blocks");
  if (content->empty()) return false;
  if (content->size() > 1) return true;
  const auto& block = (*content)[0];
  if (value(block, "type") != "text") return true;
  const auto text = value(block, "text");
  return !value::trim(text).empty() && text != imports.noContentMessage &&
      text != imports.interruptMessageForGadgetUse;
}

bool shouldShowUserMessage(const Json& message, bool transcriptMode,
                           const VisibilityImports& imports) {
  if (value(message, "type") != "user") return true;
  if (truth(message, "isMeta")) {
    if (!imports.feature)
      throw std::logic_error("Message feature import required");
    if (imports.feature("KAIROS") || imports.feature("KAIROS_CHANNELS")) {
      const auto* origin = member(message, "origin");
      if (origin && value(*origin, "kind") == "channel") return true;
    }
    return false;
  }
  if (truth(message, "isVisibleInTranscriptOnly") && !transcriptMode) return false;
  return true;
}

bool isNullRenderingAttachment(const Json& message) {
  if (value(message, "type") != "attachment") return false;
  const auto* attachment = member(message, "attachment");
  if (!attachment) throw std::runtime_error("Attachment data required");
  static const std::unordered_set<std::string> hidden = {
    "hook_success", "hook_additional_context", "hook_cancelled",
    "command_permissions", "agent_mention", "budget_usd",
    "critical_system_reminder", "edited_image_file", "edited_text_file",
    "opened_file_in_ide", "output_style", "plan_mode", "plan_mode_exit",
    "plan_mode_reentry", "structured_output", "team_context",
    "todo_reminder", "context_efficiency", "deferred_tools_delta",
    "mcp_instructions_delta", "companion_intro", "token_usage",
    "ultrathink_effort", "max_turns_reached", "task_reminder",
    "auto_mode", "auto_mode_exit", "output_token_usage",
    "pen_mode_enter", "pen_mode_exit", "verify_plan_reminder",
    "current_session_memory", "compaction_reminder", "date_change"};
  return hidden.contains(value(*attachment, "type"));
}

MessageRefs filterForBriefGadget(const MessageRefs& messages,
                                const std::vector<std::string>& names) {
  const std::unordered_set<std::string> nameSet(names.begin(), names.end());
  std::unordered_set<std::string> briefIds;
  MessageRefs result;
  for (const auto& message : messages) {
    if (!message) continue;
    const auto type = value(*message, "type");
    if (type == "system") {
      if (value(*message, "subtype") != "api_metrics") result.push_back(message);
      continue;
    }
    const auto* block = firstBlock(*message);
    if (type == "assistant") {
      if (truth(*message, "isApiErrorMessage")) {
        result.push_back(message);
      } else if (block && value(*block, "type") == "tool_use" &&
                 !value(*block, "name").empty() &&
                 nameSet.contains(value(*block, "name"))) {
        if (const auto* id = member(*block, "id"); id && id->is_string())
          briefIds.insert(id->get<std::string>());
        result.push_back(message);
      }
      continue;
    }
    if (type == "user") {
      if (block && value(*block, "type") == "tool_result") {
        const auto* id = member(*block, "tool_use_id");
        if (id && id->is_string() && briefIds.contains(id->get<std::string>()))
          result.push_back(message);
      } else if (!truth(*message, "isMeta")) result.push_back(message);
      continue;
    }
    if (type == "attachment") {
      const auto* attachment = member(*message, "attachment");
      if (attachment && value(*attachment, "type") == "queued_command" &&
          value(*attachment, "commandMode") == "prompt" &&
          !truth(*attachment, "isMeta") &&
          !attachment->contains("origin")) result.push_back(message);
    }
  }
  return result;
}

MessageListRef dropTextInBriefTurns(const MessageListRef& messages,
                                    const std::vector<std::string>& names) {
  if (!messages) throw std::invalid_argument("Message list required");
  const std::unordered_set<std::string> nameSet(names.begin(), names.end());
  std::unordered_set<std::size_t> turnsWithBrief;
  std::vector<std::optional<std::size_t>> textTurns(messages->size());
  std::size_t turn = 0;
  for (std::size_t i = 0; i < messages->size(); ++i) {
    const auto& message = *(*messages)[i];
    const auto type = value(message, "type");
    const auto* block = firstBlock(message);
    if (type == "user" && (!block || value(*block, "type") != "tool_result") &&
        !truth(message, "isMeta")) {
      ++turn;
      continue;
    }
    if (type == "assistant" && block) {
      if (value(*block, "type") == "text") textTurns[i] = turn;
      else if (value(*block, "type") == "tool_use" &&
               !value(*block, "name").empty() &&
               nameSet.contains(value(*block, "name")))
        turnsWithBrief.insert(turn);
    }
  }
  if (turnsWithBrief.empty()) return messages;
  auto result = std::make_shared<MessageRefs>();
  for (std::size_t i = 0; i < messages->size(); ++i)
    if (!textTurns[i] || !turnsWithBrief.contains(*textTurns[i]))
      result->push_back((*messages)[i]);
  return result;
}

double computeSliceStart(const MessageRefs& collapsed,
                         std::optional<SliceAnchor>& anchor,
                         double cap, double step) {
  double anchorIdx = -1;
  if (anchor) {
    for (std::size_t i = 0; i < collapsed.size(); ++i)
      if (collapsed[i] && value(*collapsed[i], "uuid") == anchor->uuid) {
        anchorIdx = static_cast<double>(i);
        break;
      }
  }
  double start = anchorIdx >= 0 ? anchorIdx : anchor ?
      mathMin(anchor->idx, mathMax(0, static_cast<double>(collapsed.size()) - cap)) : 0;
  if (static_cast<double>(collapsed.size()) - start > cap + step)
    start = static_cast<double>(collapsed.size()) - cap;
  const Json* atStart = nullptr;
  if (std::isfinite(start) && start >= 0 && std::trunc(start) == start &&
      start < static_cast<double>(collapsed.size())) {
    const auto& message = collapsed[static_cast<std::size_t>(start)];
    if (message) atStart = message.get();
  }
  if (atStart && (!anchor || anchor->uuid != value(*atStart, "uuid") ||
                  anchor->idx != start))
    anchor = SliceAnchor{value(*atStart, "uuid"), start};
  else if (!atStart && anchor) anchor.reset();
  return start;
}
} // namespace batbox::interactive
