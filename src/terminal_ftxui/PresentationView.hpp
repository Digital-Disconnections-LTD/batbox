#pragma once

#include "interactive/PresentationState.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/screen/terminal.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
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
  // Drops rows not requested since the previous sweep.
  void sweep();

private:
  struct Entry {
    std::string text;
    interactive::TranscriptRow::Kind kind = interactive::TranscriptRow::Kind::System;
    bool transient = false;
    bool valid = false;
    std::size_t width = 0;
    std::uint64_t used = 0;
    std::vector<VisualLine> lines;
  };
  std::unordered_map<std::string, Entry> entries_;
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
std::vector<VisualLine> projectTranscriptLines(
    const interactive::PresentationState& state, int columns);
std::vector<VisualLine> projectCopyLines(
    const interactive::PresentationState& state, int columns);
ftxui::Element renderPresentation(const interactive::PresentationState& state,
                                  ftxui::Dimensions dimensions);
ftxui::Element renderPresentation(const interactive::PresentationState& state,
                                  ftxui::Dimensions dimensions,
                                  const std::unordered_set<std::string>& excluded);
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
