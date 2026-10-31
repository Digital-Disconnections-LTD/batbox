#include "interactive/cards/ThinkingHighlight.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::interactive::cards {
namespace {
bool wordCharacter(char16_t value) {
  return (value >= u'A' && value <= u'Z') ||
         (value >= u'a' && value <= u'z') ||
         (value >= u'0' && value <= u'9') || value == u'_';
}
char16_t lowerAscii(char16_t value) {
  return value >= u'A' && value <= u'Z' ? value + (u'a' - u'A') : value;
}
} // namespace

std::vector<TriggerPosition> findThinkingTriggerPositions(const std::string& text) {
  const auto units = value::utf16CodeUnits(text);
  constexpr std::u16string_view trigger = u"ultrathink";
  std::vector<TriggerPosition> result;
  for (std::size_t i = 0; i + trigger.size() <= units.size(); ++i) {
    if (i && wordCharacter(units[i - 1])) continue;
    if (i + trigger.size() < units.size() &&
        wordCharacter(units[i + trigger.size()])) continue;
    bool matches = true;
    for (std::size_t j = 0; j < trigger.size(); ++j)
      if (lowerAscii(units[i + j]) != trigger[j]) {
        matches = false;
        break;
      }
    if (!matches) continue;
    result.push_back({value::fromUtf16(units.substr(i, trigger.size())),
                      i, i + trigger.size()});
    i += trigger.size() - 1;
  }
  return result;
}

std::string rainbowColor(std::size_t index, bool shimmer) {
  static constexpr const char* standard[] = {
      "rainbow_red", "rainbow_orange", "rainbow_yellow", "rainbow_green",
      "rainbow_blue", "rainbow_indigo", "rainbow_violet"};
  static constexpr const char* shifting[] = {
      "rainbow_red_shimmer", "rainbow_orange_shimmer", "rainbow_yellow_shimmer",
      "rainbow_green_shimmer", "rainbow_blue_shimmer",
      "rainbow_indigo_shimmer", "rainbow_violet_shimmer"};
  return shimmer ? shifting[index % 7] : standard[index % 7];
}

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
  const auto units = value::utf16CodeUnits(text);
  const auto triggers = options.ultrathinkEnabled ?
      findThinkingTriggerPositions(text) : std::vector<TriggerPosition>{};
  if (triggers.empty()) {
    view.content.push_back({text, "text"});
    return view;
  }
  std::size_t cursor = 0;
  for (const auto& trigger : triggers) {
    if (trigger.start > cursor)
      view.content.push_back({value::fromUtf16(units.substr(cursor,
          trigger.start - cursor)), "text"});
    for (std::size_t i = trigger.start; i < trigger.end; ++i)
      view.content.push_back({value::fromUtf16(units.substr(i, 1)),
                              rainbowColor(i - trigger.start)});
    cursor = trigger.end;
  }
  if (cursor < units.size())
    view.content.push_back({value::fromUtf16(units.substr(cursor)), "text"});
  return view;
}
} // namespace batbox::interactive::cards
