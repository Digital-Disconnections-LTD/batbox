#include "terminal_ftxui/ScrollbackProjection.hpp"
#include <algorithm>

namespace batbox::terminal_ftxui {
std::vector<CommittedRow> advanceInlineScrollback(
    const interactive::PresentationState& state, ftxui::Dimensions dimensions,
    ScrollbackCursor& cursor, bool inlineMode) {
  std::vector<CommittedRow> rows;
  if (!inlineMode || !state.viewport.follow) return rows;
  const auto full = layoutPresentation(state, dimensions.dimx, dimensions.dimy,
                                       nullptr, &cursor.lines);
  const auto width = static_cast<std::size_t>(dimensions.dimx);
  std::size_t index = 0;
  for (const auto& row : state.transcriptRows) {
    if (index >= full.topRow) break;
    std::vector<VisualLine> scratch;
    const std::vector<VisualLine>* lines = nullptr;
    if (row.scrollbackId.empty()) {
      scratch = wrapTranscriptRow(row, width);
      lines = &scratch;
    } else lines = &cursor.lines.lines(row, width);
    for (const auto& line : *lines) {
      if (index >= full.topRow) break;
      ++index;
      if (line.id.empty() || line.transient ||
          !cursor.committed.insert(line.id).second) continue;
      std::string text;
      for (const auto& cell : line.cells) text += cell;
      text = "\x1b[" + colorFor(line.kind).Print(false) + "m" + text + "\x1b[39m";
      rows.push_back({line.id, std::move(text)});
    }
  }
  return rows;
}

ftxui::Element renderPresentationWithScrollback(
    const interactive::PresentationState& state, ftxui::Dimensions dimensions,
    ScrollbackCursor& cursor, bool inlineMode) {
  if (!inlineMode) return renderPresentation(state, dimensions);
  return renderPresentation(state, dimensions, cursor.committed, cursor.lines);
}
} // namespace batbox::terminal_ftxui
