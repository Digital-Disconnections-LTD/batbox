#pragma once

#include "intl/Intl.hpp"
#include "text_width/StringWidth.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace batbox::terminal_ftxui {

// Install Batbox's grapheme segmentation and width calculation into
// the pinned FTXUI screen/string adapter. Call on the terminal owner before
// creating a Driver; the FTXUI process-wide hook retains both providers.
void installIndigenousStringMetrics(std::shared_ptr<intl::Stash> intl,
                                std::shared_ptr<text_width::StringWidth> width);

// The installed glyph splitter: ASCII text is split byte by byte, other text
// through grapheme segmentation (segmentedGlyphCells). Exposed for tests.
std::vector<std::string> indigenousGlyphCells(const intl::Segmenter& segmenter,
    const text_width::StringWidth& width, std::string_view text);
std::vector<std::string> segmentedGlyphCells(const intl::Segmenter& segmenter,
    const text_width::StringWidth& width, std::string_view text);

} // namespace batbox::terminal_ftxui
