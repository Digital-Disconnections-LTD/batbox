#pragma once

#include "terminal_ftxui/Driver.hpp"
#include "terminal_ftxui/PresentationView.hpp"

namespace batbox::terminal_ftxui {

struct ScrollbackCursor {
  std::unordered_set<std::string> committed;
};

std::vector<CommittedRow> advanceInlineScrollback(
    const interactive::PresentationState& state, ftxui::Dimensions dimensions,
    ScrollbackCursor& cursor, bool inlineMode);

ftxui::Element renderPresentationWithScrollback(
    const interactive::PresentationState& state, ftxui::Dimensions dimensions,
    const ScrollbackCursor& cursor, bool inlineMode);

} // namespace batbox::terminal_ftxui
