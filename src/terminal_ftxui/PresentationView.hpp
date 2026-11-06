#pragma once

#include "interactive/PresentationState.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/screen/terminal.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace batbox::terminal_ftxui {

struct VisualLine {
  interactive::TranscriptRow::Kind kind = interactive::TranscriptRow::Kind::System;
  std::vector<std::string> cells;
  bool selected = false;
  std::string id;
  bool transient = false;
  bool hardBreakAfter = false;
};
ftxui::Color colorFor(interactive::TranscriptRow::Kind kind);
// Row colors for inline mode. The host installs colors from the active theme;
// the defaults use the terminal's basic palette.
struct InlinePalette {
  ftxui::Color user = ftxui::Color::Yellow;
  ftxui::Color assistant = ftxui::Color::Default;
  ftxui::Color reasoning = ftxui::Color::GrayDark;
  ftxui::Color gadget = ftxui::Color::Magenta;
  ftxui::Color gadgetResult = ftxui::Color::GrayDark;
  ftxui::Color progress = ftxui::Color::GrayDark;
  ftxui::Color system = ftxui::Color::Cyan;
  ftxui::Color error = ftxui::Color::Red;
  ftxui::Color queued = ftxui::Color::GrayDark;
};
void setInlinePalette(const InlinePalette& palette);
struct PresentationLayout {
  std::vector<VisualLine> visibleLines;
  std::size_t transcriptRowCount = 0;
  std::size_t topRow = 0;
  std::size_t bodyHeight = 0;
  int caretX = 0, caretY = 0;
  bool caretVisible = false;
};

// Wrapped rows keyed by scrollback id. The inline view re-wraps only rows
// whose text or width changed since the previous frame; rows without an id
// are wrapped by the caller.
class TranscriptLineCache {
public:
  const std::vector<VisualLine>& lines(const interactive::TranscriptRow& row,
                                       std::size_t width);
  // The number of lines, without wrapping a committed row's text again.
  std::size_t lineCount(const interactive::TranscriptRow& row, std::size_t width);
  // Whether every line of the row is in the committed set kept beside this
  // cache (ScrollbackCursor). A row wrapped again starts uncommitted.
  // Committed rows keep only their line count; the terminal holds the text.
  bool committed(const interactive::TranscriptRow& row) const;
  void markCommitted(const interactive::TranscriptRow& row);
  // A live row `lead + text` whose text only grows (the streaming preview, a
  // Gadget's partial input), wrapped as wrapTranscriptRow wraps it. Lines
  // already wrapped are kept; only appended text is wrapped. The row is
  // `head`'s first `headCount` lines followed by `tail`.
  struct Growing {
    const std::vector<VisualLine>* head = nullptr;
    std::size_t headCount = 0;
    const std::vector<VisualLine>* tail = nullptr;
  };
  Growing growing(const std::string& key, interactive::TranscriptRow::Kind kind,
                  std::string_view lead, std::string_view text, std::size_t width);
  // Drops rows not requested since the previous sweep.
  void sweep();

private:
  struct Entry {
    // Rows with a revision are matched by it; others by their text.
    std::uint64_t revision = 0;
    std::string text;
    interactive::TranscriptRow::Kind kind = interactive::TranscriptRow::Kind::System;
    bool transient = false;
    bool valid = false;
    bool committed = false;
    std::size_t width = 0, count = 0;
    std::uint64_t used = 0;
    std::vector<VisualLine> lines;
  };
  Entry& refresh(const interactive::TranscriptRow& row, std::size_t width);
  struct GrowingEntry {
    std::string lead,source;
    // Bytes of source already wrapped into `lines`; the rest is re-wrapped
    // into `tail` on each call.
    std::size_t wrapped = 0;
    interactive::TranscriptRow::Kind kind = interactive::TranscriptRow::Kind::System;
    std::size_t width = 0;
    std::uint64_t used = 0;
    std::vector<VisualLine> lines, tail;
  };
  std::unordered_map<std::string, Entry> entries_;
  std::unordered_map<std::string, GrowingEntry> growing_;
  std::uint64_t generation_ = 0;
  std::size_t touched_ = 0;
};
std::vector<VisualLine> wrapTranscriptRow(const interactive::TranscriptRow& row,
                                          std::size_t width);
PresentationLayout layoutPresentation(const interactive::PresentationState& state,
                                      int columns, int rows,
                                      const std::unordered_set<std::string>* excluded = nullptr);
PresentationLayout layoutPresentation(const interactive::PresentationState& state,
                                      int columns, int rows,
                                      const std::unordered_set<std::string>* excluded,
                                      TranscriptLineCache* cache);
// Line counts and the top row of the body without copying any lines; the
// visible lines are left empty.
PresentationLayout layoutPresentationHeights(const interactive::PresentationState& state,
                                             int columns, int rows,
                                             TranscriptLineCache& cache);
// The first body line the inline view shows, without copying any lines.
std::size_t layoutTopRow(const interactive::PresentationState& state,
                         int columns, int rows, TranscriptLineCache& cache);
std::vector<VisualLine> projectCopyLines(
    const interactive::PresentationState& state, int columns);
ftxui::Element renderPresentation(const interactive::PresentationState& state,
                                  ftxui::Dimensions dimensions);
ftxui::Element renderPresentation(const interactive::PresentationState& state,
                                  ftxui::Dimensions dimensions,
                                  const std::unordered_set<std::string>& excluded,
                                  TranscriptLineCache& cache);

struct PlainCursor {
  std::vector<interactive::MessageRef> emitted;
  // Identities of `emitted`; the references above keep them unique.
  std::unordered_set<const Json*> emittedIds;
};
std::string drainPresentationPlain(const interactive::PresentationState& state,
                                   PlainCursor& cursor);

} // namespace batbox::terminal_ftxui
