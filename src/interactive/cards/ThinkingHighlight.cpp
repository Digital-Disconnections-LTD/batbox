#include "interactive/cards/ThinkingHighlight.hpp"
#include <stdexcept>

namespace batbox::interactive::cards {
HighlightedTextView highlightedThinkingText(
    const std::string& text, bool brief,
    const std::optional<std::string>& timestamp,
    const HighlightOptions& options) {
  HighlightedTextView view;
  view.brief = brief;
  if (brief) {
    const auto ts = timestamp && !timestamp->empty() ?
        (options.formatBriefTimestamp ? options.formatBriefTimestamp(*timestamp) :
         throw std::logic_error("Brief timestamp formatter required")) : "";
    view.heading.push_back({"You", options.queued ? "subtle" : "briefLabelYou"});
    if (!ts.empty()) view.heading.push_back({" " + ts, "dim"});
    view.content.push_back({text, options.queued ? "subtle" : "text"});
    return view;
  }
  view.content.push_back({options.pointerGlyph + " ",
                          options.selected ? "suggestion" : "subtle"});
  view.content.push_back({text, "text"});
  return view;
}
} // namespace batbox::interactive::cards
