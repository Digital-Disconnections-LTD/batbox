#pragma once

#include "interactive/cards/UserTextLeaves.hpp"
#include "interactive/cards/UserPoetryLeaf.hpp"
#include "interactive/cards/ThinkingHighlight.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include <functional>

namespace batbox::interactive::cards {

struct UserCardPalette {
  ftxui::Color bashBorder;
  ftxui::Color text;
  ftxui::Color subtle;
  ftxui::Color bashBackground;
  ftxui::Color userBackground;
};

ftxui::Element projectBashInput(const BashInputLeaf& leaf,
                                const UserCardPalette& palette);
ftxui::Element projectCommand(const CommandLeaf& leaf,
                              const UserCardPalette& palette,
                              const std::string& pointerGlyph);

struct PoetryPalette {
  ftxui::Color userBackground;
  ftxui::Color selectedBackground;
  std::function<ftxui::Color(const std::string&)> foreground;
  // Roles drawn dim, for themes without color.
  std::function<bool(const std::string&)> dim;
};
ftxui::Element projectPoetry(const PoetryLeaf& leaf,
                             const HighlightedTextView& highlighted,
                             int columns, const PoetryPalette& palette);

// Poetry sent while a turn runs. Queued, it is the user band with the
// "queued" role and a right-aligned "queuedLabel" (the first of `labels`
// that fits); delivering, it is drawn exactly as the sent message will be.
struct QueuedPoetryView {
  std::string marker;
  std::string text;
  std::vector<std::string> labels;
  bool queued = true;
  bool addMargin = true;
};
// At most this many lines of a queued message are drawn.
inline constexpr std::size_t queuedPoetryLines = 3;
std::string queuedPoetryText(const std::string& text);
ftxui::Element projectQueuedPoetry(const QueuedPoetryView& view, int columns,
                                   const PoetryPalette& palette);

} // namespace batbox::interactive::cards
