#pragma once

#include "interactive/cards/OutputLineCard.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

namespace batbox::interactive::cards {

struct OutputLinePalette {
  ftxui::Color prefix;
  ftxui::Color text;
  ftxui::Color error;
  ftxui::Color warning;
};

// MessageResponse + Ansi projection for one already formatted OutputLine.
ftxui::Element projectOutputLine(const OutputLineCard& line, int columns,
                                 const OutputLinePalette& palette,
                                 bool singleLine = false);

} // namespace batbox::interactive::cards
