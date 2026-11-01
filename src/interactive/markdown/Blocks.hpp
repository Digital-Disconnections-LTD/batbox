#pragma once

#include "interactive/markdown/Format.hpp"
#include <ftxui/dom/elements.hpp>

namespace batbox::interactive::markdown {

// Fenced code: a rounded frame in the code-frame role with the language in
// its top edge, the code-background tint inside, highlighted runs and hard
// wrapping that keeps leading indentation.
ftxui::Element projectCodeBlock(const Token& code,const FormatServices& services,
    const FormatServices::BlockStyle& style,int columns,bool dim);
// A thematic break drawn as a light rule across the reading width.
ftxui::Element projectRule(const FormatServices::BlockStyle& style,int columns);
// Lists with markers in their own column, so wrapped text hangs under the
// item text instead of under the marker.
ftxui::Element projectList(const Token& list,const FormatServices& services,
    const FormatServices::BlockStyle& style,bool dim,int depth=0);

} // namespace batbox::interactive::markdown
