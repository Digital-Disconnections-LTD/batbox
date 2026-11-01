#pragma once

#include <ftxui/dom/elements.hpp>
#include <string>
#include <string_view>

namespace batbox::terminal_ftxui {

// Terminal display width of UTF-8 text (fullwidth glyphs count as two).
int displayWidth(std::string_view text);
// Cuts at a glyph boundary so the result plus the ellipsis fits in columns.
std::string truncateColumns(std::string_view text,int columns,
    std::string_view ellipsis="…");
// One row of text that ends with an ellipsis when its box is narrower than
// the text. It asks for its full width and is the first thing an hbox shrinks.
ftxui::Element fitText(std::string text);

} // namespace batbox::terminal_ftxui
