#pragma once

#include "interactive/PresentationState.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/screen/terminal.hpp"
#include <cstddef>
#include <string>
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

PresentationLayout layoutPresentation(const interactive::PresentationState& state,
                                      int columns, int rows,
                                      const std::unordered_set<std::string>* excluded = nullptr);
std::vector<VisualLine> projectTranscriptLines(
    const interactive::PresentationState& state, int columns);
std::vector<VisualLine> projectCopyLines(
    const interactive::PresentationState& state, int columns);
ftxui::Element renderPresentation(const interactive::PresentationState& state,
                                  ftxui::Dimensions dimensions);
ftxui::Element renderPresentation(const interactive::PresentationState& state,
                                  ftxui::Dimensions dimensions,
                                  const std::unordered_set<std::string>& excluded);

struct PlainCursor {
  std::vector<interactive::MessageRef> emitted;
  // Identities of `emitted`; the references above keep them unique.
  std::unordered_set<const Json*> emittedIds;
};
std::string drainPresentationPlain(const interactive::PresentationState& state,
                                   PlainCursor& cursor);

} // namespace batbox::terminal_ftxui
