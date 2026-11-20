#include "interactive/cards/ThinkingHighlight.hpp"
#include <stdexcept>

namespace batbox::interactive::cards {
HighlightedTextView highlightedThinkingText(
    const std::string& text, const HighlightOptions& options) {
  HighlightedTextView view;
  view.content.push_back({options.pointerGlyph + " ",
                          options.selected ? "suggestion" : "subtle"});
  view.content.push_back({text, "text"});
  return view;
}
} // namespace batbox::interactive::cards
