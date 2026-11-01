#include "terminal_ftxui/PresentationView.hpp"
#include "interactive/TranscriptRows.hpp"
#include "interactive/gadget_views/GadgetViewModel.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include "terminal_ftxui/StyleProjection.hpp"
#include "ftxui/screen/string.hpp"
#include <algorithm>
#include <string_view>
#include <unordered_map>

namespace batbox::terminal_ftxui {
namespace {
using Kind = interactive::TranscriptRow::Kind;

std::string prefix(Kind kind) {
  switch (kind) {
    case Kind::User: return "❯ ";
    case Kind::Assistant: return "  ";
    case Kind::Reasoning: return "∴ ";
    case Kind::Gadget: return "● ";
    case Kind::GadgetResult: return "⎿ ";
    case Kind::Progress: return "· ";
    case Kind::System: return "· ";
    case Kind::Error: return "✗ ";
  }
  return "  ";
}

void nextLine(std::vector<VisualLine>& lines, Kind kind, bool selected = false) {
  lines.push_back({kind, {}, selected, {}, false});
}

void appendGlyph(std::vector<VisualLine>& lines, const std::string& glyph,
                 bool wideLead, std::size_t width, Kind kind, bool selected) {
  if (wideLead && width < 2)
    throw ProjectionError("Viewport too narrow for a fullwidth glyph");
  if (lines.empty()) nextLine(lines, kind, selected);
  auto& line = lines.back();
  if (line.cells.size() + (wideLead ? 2 : 1) > width)
    nextLine(lines, kind, selected);
  lines.back().cells.push_back(glyph);
}

void appendChunk(std::vector<VisualLine>& lines, std::string_view chunk,
                 std::size_t width, Kind kind, bool selected) {
  const auto glyphs = ftxui::Utf8ToGlyphs(chunk);
  for (std::size_t i = 0; i < glyphs.size(); ++i) {
    const bool wideLead = !glyphs[i].empty() && i + 1 < glyphs.size() &&
        glyphs[i + 1].empty();
    if (!lines.empty() && !glyphs[i].empty() && glyphs[i] != " " &&
        lines.back().cells.size() + (wideLead ? 2 : 1) > width) {
      // Wrap at the last space so words stay whole; the space stays on the
      // upper row, so joining soft-wrapped rows restores the text.
      auto& cells = lines.back().cells;
      std::size_t space = cells.size();
      while (space > 0 && cells[space - 1] != " ") --space;
      const auto tail = cells.size() - space;
      if (space > 1 && tail > 0 && tail < width / 2) {
        std::vector<std::string> moved(cells.begin() + static_cast<std::ptrdiff_t>(space),
                                       cells.end());
        cells.resize(space);
        nextLine(lines, kind, selected);
        lines.back().cells = std::move(moved);
      }
    }
    appendGlyph(lines, glyphs[i], wideLead, width, kind, selected);
  }
}

std::vector<VisualLine> wrap(std::string_view text, std::string_view firstPrefix,
                             std::size_t width, Kind kind, bool selected = false) {
  std::vector<VisualLine> lines;
  nextLine(lines, kind, selected);
  // Newlines form visible rows; tabs use the terminal's eight-column
  // stops. FTXUI's glyph conversion handles combining and fullwidth cells.
  const std::string combined = std::string(firstPrefix) + std::string(text);
  text = combined;
  std::size_t start = 0;
  while (start <= text.size()) {
    const auto next = text.find_first_of("\n\t", start);
    appendChunk(lines, text.substr(start, next - start), width, kind, selected);
    if (next == std::string_view::npos) break;
    if (text[next] == '\n') {
      lines.back().hardBreakAfter = true;
      nextLine(lines, kind, selected);
    }
    else {
      auto spaces = 8 - (lines.back().cells.size() % 8);
      while (spaces-- > 0)
        appendGlyph(lines, " ", false, width, kind, selected);
    }
    start = next + 1;
  }
  return lines;
}

// Gadget output can be a whole file; the inline view keeps the first rows.
std::string visibleRowText(const interactive::TranscriptRow& row) {
  if (row.kind == Kind::Error)
    return interactive::gadget_views::sanitize(row.text);
  if (row.kind != Kind::GadgetResult) return row.text;
  const auto text = interactive::gadget_views::sanitize(row.text);
  constexpr std::size_t shown = 8;
  std::size_t lines = 1, cut = std::string::npos;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '\n') continue;
    if (lines == shown && cut == std::string::npos) cut = i;
    ++lines;
  }
  if (!text.empty() && text.back() == '\n') --lines;
  if (lines <= shown || cut == std::string::npos) return text;
  const auto hidden = lines - shown;
  return text.substr(0, cut) + "\n… +" + std::to_string(hidden) +
      (hidden == 1 ? " line" : " lines");
}

void appendWrapped(std::vector<VisualLine>& destination,
                   const interactive::TranscriptRow& row, std::size_t width,
                   const std::string& rowId = {}) {
  auto lines = wrap(value::encodeUtf8(visibleRowText(row)), prefix(row.kind), width, row.kind);
  for (std::size_t i = 0; i < lines.size(); ++i) {
    if (!rowId.empty()) lines[i].id = rowId + ":" + std::to_string(i);
    lines[i].transient = row.transient;
  }
  destination.insert(destination.end(),
                     std::make_move_iterator(lines.begin()),
                     std::make_move_iterator(lines.end()));
}

std::vector<VisualLine> inputLines(const interactive::InputState& input,
                                   std::size_t width) {
  return wrap(value::encodeUtf8(input.text), value::encodeUtf8(input.poetry),
              width, Kind::User);
}

std::vector<VisualLine> modalLines(const interactive::UiRequest& modal,
                                   std::size_t width) {
  std::vector<VisualLine> lines;
  auto title = wrap(value::encodeUtf8(modal.title), "✦ ", width, Kind::System);
  lines.insert(lines.end(), std::make_move_iterator(title.begin()),
               std::make_move_iterator(title.end()));
  if (!modal.body.empty()) {
    auto body = wrap(value::encodeUtf8(modal.body), "  ", width, Kind::System);
    lines.insert(lines.end(), std::make_move_iterator(body.begin()),
                 std::make_move_iterator(body.end()));
  }
  for (std::size_t i = 0; i < modal.choices.size(); ++i) {
    const bool selected = i == modal.selectedIndex;
    auto choice = wrap(value::encodeUtf8(modal.choices[i]),
                       selected ? "❯ " : "  ", width, Kind::System, selected);
    lines.insert(lines.end(), std::make_move_iterator(choice.begin()),
                 std::make_move_iterator(choice.end()));
  }
  return lines;
}

std::string status(const interactive::PresentationState& state) {
  if (!state.loading) return {};
  if (!state.streamMode) return "Working…";
  if (*state.streamMode == "requesting") return "Connecting…";
  if (*state.streamMode == "thinking") return "Thinking…";
  if (*state.streamMode == "responding") return "Writing…";
  if (*state.streamMode == "tool-input") return "Preparing a Gadget…";
  if (*state.streamMode == "tool-use") return "Running Gadgets…";
  return "Working…";
}
} // namespace

std::vector<VisualLine> projectTranscriptLines(
    const interactive::PresentationState& state, int columns) {
  if (columns <= 0) throw ProjectionError("Positive FTXUI viewport required");
  std::vector<VisualLine> body;
  for (std::size_t i = 0; i < state.transcriptRows.size(); ++i) {
    const auto& row = state.transcriptRows[i];
    appendWrapped(body, row, static_cast<std::size_t>(columns), row.scrollbackId);
  }
  return body;
}

std::vector<VisualLine> projectCopyLines(
    const interactive::PresentationState& state, int columns) {
  if (columns <= 0) throw ProjectionError("Positive copy viewport required");
  std::vector<VisualLine> body;
  for (const auto& row : state.transcriptRows) {
    auto lines = wrap(value::encodeUtf8(row.text), "",
                      static_cast<std::size_t>(columns), row.kind);
    if (!lines.empty()) lines.back().hardBreakAfter = true;
    body.insert(body.end(), std::make_move_iterator(lines.begin()),
                std::make_move_iterator(lines.end()));
  }
  return body;
}

PresentationLayout layoutPresentation(const interactive::PresentationState& state,
                                      int columns, int rows,
                                      const std::unordered_set<std::string>* excluded) {
  if (columns <= 0 || rows <= 0)
    throw ProjectionError("Positive FTXUI viewport required");
  const auto width = static_cast<std::size_t>(columns);
  const auto height = static_cast<std::size_t>(rows);
  std::vector<VisualLine> body = projectTranscriptLines(state, columns);
  if (excluded) body.erase(std::remove_if(body.begin(), body.end(),
      [excluded](const VisualLine& line) {
        return !line.id.empty() && excluded->contains(line.id);
      }), body.end());
  if (const auto preview = interactive::visibleStreamingText(state))
    appendWrapped(body, {Kind::Assistant, "", *preview, true, {}}, width);
  for (const auto& gadget : state.streamingGadgetUses) {
    std::string text = gadget.contentBlock.is_object() &&
        gadget.contentBlock.contains("name") &&
        gadget.contentBlock["name"].is_string() ?
        gadget.contentBlock["name"].get<std::string>() : "Gadget";
    if (!gadget.unparsedGadgetInput.empty())
      text += " " + gadget.unparsedGadgetInput;
    appendWrapped(body, {Kind::Gadget, "", std::move(text), true, {}}, width);
  }

  const bool showInput = !state.modal || !state.modal->hidePoetry;
  const auto fullInput = showInput ? inputLines(state.input, width) :
      std::vector<VisualLine>{};
  const auto fullModal = state.modal ? modalLines(*state.modal, width) :
      std::vector<VisualLine>{};
  const auto statusText = status(state);
  const std::size_t statusHeight = !statusText.empty() && height > 1 ? 1 : 0;
  const std::size_t inputHeight = showInput ?
      std::min(fullInput.size(), std::min<std::size_t>(5, height - statusHeight)) : 0;
  const std::size_t modalHeight = state.modal ?
      std::min(fullModal.size(), std::min<std::size_t>(height / 2 + 1,
          height - statusHeight - inputHeight)) : 0;
  const std::size_t bodyHeight = height - statusHeight - inputHeight - modalHeight;
  const std::size_t maxTop = body.size() > bodyHeight ? body.size() - bodyHeight : 0;
  const std::size_t top = state.viewport.follow ? maxTop :
      std::min(state.viewport.topRow, maxTop);

  PresentationLayout layout;
  layout.transcriptRowCount = body.size();
  layout.bodyHeight = bodyHeight;
  layout.topRow = top;
  for (std::size_t i = top; i < std::min(body.size(), top + bodyHeight); ++i)
    layout.visibleLines.push_back(std::move(body[i]));
  while (layout.visibleLines.size() < bodyHeight)
    layout.visibleLines.push_back({Kind::System, {}, false, {}, false});
  if (!statusText.empty() && statusHeight) {
    auto line = wrap(statusText, "", width, Kind::Progress);
    if (!line.empty()) layout.visibleLines.push_back(std::move(line.front()));
  }
  if (modalHeight) {
    auto start = fullModal.size() - modalHeight;
    for (std::size_t i = 0; i < fullModal.size(); ++i) {
      if (!fullModal[i].selected) continue;
      if (i < start) start = i;
      else if (i >= start + modalHeight) start = i + 1 - modalHeight;
      break;
    }
    for (std::size_t i = start; i < fullModal.size(); ++i)
      if (i < start + modalHeight) layout.visibleLines.push_back(fullModal[i]);
  }
  if (inputHeight) {
    const auto prefix = value::substring(state.input.text, 0,
        static_cast<double>(state.input.caretUtf16));
    const auto caretLines = wrap(value::encodeUtf8(prefix),
        value::encodeUtf8(state.input.poetry), width, Kind::User);
    const auto caretLine = caretLines.empty() ? 0 : caretLines.size() - 1;
    const auto start = caretLine + 1 > inputHeight ? caretLine + 1 - inputHeight : 0;
    const auto inputStartY = layout.visibleLines.size();
    for (std::size_t i = start; i < std::min(fullInput.size(), start + inputHeight); ++i)
      layout.visibleLines.push_back(fullInput[i]);
    if (state.input.focused && caretLine >= start &&
        caretLine < start + inputHeight) {
      layout.caretX = static_cast<int>(std::min(width - 1,
          caretLines.back().cells.size()));
      layout.caretY = static_cast<int>(inputStartY + caretLine - start);
      layout.caretVisible = true;
    }
  }
  return layout;
}
} // namespace batbox::terminal_ftxui
