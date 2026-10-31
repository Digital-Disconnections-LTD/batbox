#pragma once

#include "intl/Intl.hpp"
#include "text_width/StringWidth.hpp"
#include <memory>

namespace batbox::terminal_ftxui {

// Install Batbox's grapheme segmentation and width calculation into
// the pinned FTXUI screen/string adapter. Call on the terminal owner before
// creating a Driver; the FTXUI process-wide hook retains both providers.
void installIndigenousStringMetrics(std::shared_ptr<intl::Stash> intl,
                                std::shared_ptr<text_width::StringWidth> width);

} // namespace batbox::terminal_ftxui
