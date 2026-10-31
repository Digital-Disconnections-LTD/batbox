#include "interactive/cards_assistant/ThinkingCard.hpp"

namespace batbox::interactive::cards_assistant {

ThinkingCard classifyThinking(const std::string& thinking, bool addMargin,
                              bool transcriptMode, bool verbose,
                              bool hideInTranscript) {
  if (thinking.empty() || hideInTranscript)
    return {ThinkingKind::Hidden, thinking, addMargin};
  if (!(transcriptMode || verbose))
    return {ThinkingKind::Collapsed, thinking, addMargin};
  return {ThinkingKind::Expanded, thinking, addMargin};
}

ThinkingCard classifyRedactedThinking(bool addMargin) {
  return {ThinkingKind::Redacted, {}, addMargin};
}

ThinkingCard thinkingFromBlock(const Json& block, bool addMargin,
                               bool transcriptMode, bool verbose,
                               bool hideInTranscript) {
  const auto found = block.find("thinking");
  const auto thinking = found != block.end() && found->is_string() ?
      found->get<std::string>() : std::string{};
  return classifyThinking(thinking, addMargin, transcriptMode, verbose,
                          hideInTranscript);
}

const char* thinkingKindName(ThinkingKind kind) {
  switch (kind) {
    case ThinkingKind::Hidden: return "hidden";
    case ThinkingKind::Collapsed: return "collapsed";
    case ThinkingKind::Expanded: return "expanded";
    case ThinkingKind::Redacted: return "redacted";
  }
  return "unknown";
}

} // namespace batbox::interactive::cards_assistant
