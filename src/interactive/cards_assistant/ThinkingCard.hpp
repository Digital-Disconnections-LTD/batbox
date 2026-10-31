#pragma once

#include "runtime/JsonValue.hpp"
#include <string>

namespace batbox::interactive::cards_assistant {

enum class ThinkingKind { Hidden, Collapsed, Expanded, Redacted };

struct ThinkingCard {
  ThinkingKind kind = ThinkingKind::Hidden;
  std::string thinking;
  bool addMargin = false;
};

ThinkingCard classifyThinking(const std::string& thinking, bool addMargin,
                              bool transcriptMode, bool verbose,
                              bool hideInTranscript);
ThinkingCard classifyRedactedThinking(bool addMargin);

// The original content block is retained until the display decision is made.
ThinkingCard thinkingFromBlock(const Json& block, bool addMargin,
                               bool transcriptMode, bool verbose,
                               bool hideInTranscript);
const char* thinkingKindName(ThinkingKind kind);

} // namespace batbox::interactive::cards_assistant
