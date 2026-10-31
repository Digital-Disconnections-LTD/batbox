#include "terminal_ftxui/ScrollbackProjection.hpp"
#include <algorithm>

namespace batbox::terminal_ftxui {
std::vector<CommittedRow> advanceInlineScrollback(
    const interactive::PresentationState& state, ftxui::Dimensions dimensions,
    ScrollbackCursor& cursor, bool inlineMode) {
  std::vector<CommittedRow> rows;
  if (!inlineMode || !state.viewport.follow) return rows;
  const auto full = layoutPresentation(state, dimensions.dimx, dimensions.dimy);
  const auto lines = projectTranscriptLines(state, dimensions.dimx);
  const auto count = std::min(full.topRow, lines.size());
  for (std::size_t i = 0; i < count; ++i) {
    const auto& line = lines[i];
    if (line.id.empty() || line.transient ||
        !cursor.committed.insert(line.id).second) continue;
    std::string text;
    for (const auto& cell : line.cells) text += cell;
    text = "\x1b[" + colorFor(line.kind).Print(false) + "m" + text + "\x1b[39m";
    rows.push_back({line.id, std::move(text)});
  }
  return rows;
}

ftxui::Element renderPresentationWithScrollback(
    const interactive::PresentationState& state, ftxui::Dimensions dimensions,
    const ScrollbackCursor& cursor, bool inlineMode) {
  if (!inlineMode) return renderPresentation(state, dimensions);
  return renderPresentation(state, dimensions, cursor.committed);
}
} // namespace batbox::terminal_ftxui
