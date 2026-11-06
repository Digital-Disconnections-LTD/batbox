#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::interactive::cards {

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
  std::function<std::string(const std::string&)> formatBriefTimestamp;
  std::string pointerGlyph;
};
HighlightedTextView highlightedThinkingText(
    const std::string& text, bool brief,
    const std::optional<std::string>& timestamp,
    const HighlightOptions& options);

} // namespace batbox::interactive::cards
