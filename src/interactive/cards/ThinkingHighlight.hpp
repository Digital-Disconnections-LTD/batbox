#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::interactive::cards {

struct TriggerPosition {
  std::string word;
  std::size_t start = 0;
  std::size_t end = 0;
};
std::vector<TriggerPosition> findThinkingTriggerPositions(const std::string& text);
std::string rainbowColor(std::size_t charIndex, bool shimmer = false);

struct HighlightPart {
  std::string text;
  std::string color;
};
struct HighlightedTextView {
  bool brief = false;
  std::vector<HighlightPart> heading;
  std::vector<HighlightPart> content;
};
struct HighlightOptions {
  bool queued = false;
  bool selected = false;
  bool ultrathinkEnabled = false;
  std::function<std::string(const std::string&)> formatBriefTimestamp;
  std::string pointerGlyph;
};
HighlightedTextView highlightedThinkingText(
    const std::string& text, bool brief,
    const std::optional<std::string>& timestamp,
    const HighlightOptions& options);

} // namespace batbox::interactive::cards
