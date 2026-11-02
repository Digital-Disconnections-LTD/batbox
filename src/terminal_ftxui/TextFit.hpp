#pragma once

#include <ftxui/dom/elements.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace batbox::terminal_ftxui {

// Terminal display width of UTF-8 text (fullwidth glyphs count as two).
int displayWidth(std::string_view text);
// Cuts at a glyph boundary so the result plus the ellipsis fits in columns.
std::string truncateColumns(std::string_view text,int columns,
    std::string_view ellipsis="…");
// Word wrap at display columns. Words wider than a row are split between
// graphemes; a grapheme wider than the row gets a row of its own. Malformed
// UTF-8 is shown as U+FFFD.
std::vector<std::string> wrapColumns(std::string_view line,int width);
// One row of text that ends with an ellipsis when its box is narrower than
// the text. It asks for its full width and is the first thing an hbox shrinks.
ftxui::Element fitText(std::string text);

} // namespace batbox::terminal_ftxui
