#pragma once

#include "interactive/MessageDecisions.hpp"
#include <string>

namespace batbox::interactive::cards_gadget_result {

enum class HookProgressKind { Hidden, TranscriptSummary, Running };

struct HookProgressCard {
  HookProgressKind kind = HookProgressKind::Hidden;
  int inProgressCount = 0;
  std::string hookEvent;
  std::string text;
};

HookProgressCard classifyHookProgress(
    const std::string& hookEvent, const std::string& gadgetUseID,
    bool transcriptMode, const MessageDecisionLookups& lookups);
const char* hookProgressKindName(HookProgressKind kind);

} // namespace batbox::interactive::cards_gadget_result
