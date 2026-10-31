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
};
ftxui::Element projectPoetry(const PoetryLeaf& leaf,
                             const HighlightedTextView& highlighted,
                             int columns, const PoetryPalette& palette);

} // namespace batbox::interactive::cards
