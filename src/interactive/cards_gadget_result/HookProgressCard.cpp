#include "interactive/cards_gadget_result/HookProgressCard.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_gadget_result {
namespace {
int count(const std::unordered_map<std::string,
              std::unordered_map<std::string,int>>& counts,
          const std::string& gadgetUseID, const std::string& event) {
  const auto gadget = counts.find(gadgetUseID);
  if (gadget == counts.end()) return 0;
  const auto found = gadget->second.find(event);
  return found == gadget->second.end() ? 0 : found->second;
}
}

HookProgressCard classifyHookProgress(
    const std::string& hookEvent, const std::string& gadgetUseID,
    bool transcriptMode, const MessageDecisionLookups& lookups) {
  const int inProgress = count(lookups.inProgressHookCounts,gadgetUseID,hookEvent);
  const int resolved = count(lookups.resolvedHookCounts,gadgetUseID,hookEvent);
  if (inProgress == 0) return {};
  if (hookEvent == "PreToolUse" || hookEvent == "PostToolUse") {
    if (!transcriptMode) return {};
    return {HookProgressKind::TranscriptSummary,inProgress,hookEvent,
        std::to_string(inProgress) + " " + hookEvent +
        (inProgress == 1 ? " hook ran" : " hooks ran")};
  }
  if (resolved == inProgress) return {};
  return {HookProgressKind::Running,inProgress,hookEvent,
      "Running " + hookEvent +
      (inProgress == 1 ? " hook…" : " hooks…")};
}

const char* hookProgressKindName(HookProgressKind kind) {
  switch (kind) {
    case HookProgressKind::Hidden: return "hidden";
    case HookProgressKind::TranscriptSummary: return "transcript_summary";
    case HookProgressKind::Running: return "running";
  }
  throw std::logic_error("Unknown hook progress kind");
}

} // namespace batbox::interactive::cards_gadget_result
