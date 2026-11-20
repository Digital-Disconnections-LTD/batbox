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
  std::vector<HighlightPart> content;
};
struct HighlightOptions {
  bool selected = false;
  std::string pointerGlyph;
};
HighlightedTextView highlightedThinkingText(
    const std::string& text, const HighlightOptions& options);

} // namespace batbox::interactive::cards
