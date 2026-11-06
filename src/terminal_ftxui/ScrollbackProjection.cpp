#include "terminal_ftxui/ScrollbackProjection.hpp"
#include <algorithm>

namespace batbox::terminal_ftxui {
std::vector<CommittedRow> advanceInlineScrollback(
    const interactive::PresentationState& state, ftxui::Dimensions dimensions,
    ScrollbackCursor& cursor, bool inlineMode) {
  std::vector<CommittedRow> rows;
  if (!inlineMode || !state.viewport.follow) return rows;
  const auto topRow = layoutTopRow(state, dimensions.dimx, dimensions.dimy, cursor.lines);
  const auto width = static_cast<std::size_t>(dimensions.dimx);
  std::size_t index = 0;
  for (const auto& row : state.transcriptRows) {
    if (index >= topRow) break;
    std::vector<VisualLine> scratch;
    const std::vector<VisualLine>* lines = nullptr;
    const bool cached = !row.scrollbackId.empty();
    if (!cached) {
      scratch = wrapTranscriptRow(row, width);
      lines = &scratch;
    } else {
      // Rows already in scrollback are counted, not looked up line by line.
      if (cursor.lines.committed(row)) {index += cursor.lines.lineCount(row, width); continue;}
      lines = &cursor.lines.lines(row, width);
    }
    bool all = true;
    for (const auto& line : *lines) {
      if (index >= topRow) {all = false; break;}
      ++index;
      if (line.id.empty() || line.transient) {all = false; continue;}
      if (!cursor.committed.insert(line.id).second) continue;
      std::string text;
      for (const auto& cell : line.cells) text += cell;
      text = "\x1b[" + colorFor(line.kind).Print(false) + "m" + text + "\x1b[39m";
      rows.push_back({line.id, std::move(text)});
    }
    if (cached && all) cursor.lines.markCommitted(row);
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
