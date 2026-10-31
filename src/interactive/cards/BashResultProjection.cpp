#include "interactive/cards/BashResultProjection.hpp"
#include "shell/FormatDuration.hpp"
#include <stdexcept>
#include <vector>

namespace batbox::interactive::cards {

ftxui::Element projectBashResult(const BashResultCard& card,
                                  const BashResultProjectionImports& imports) {
  auto dimPalette = imports.outputPalette;
  dimPalette.text = imports.dimColor;
  if (card.image)
    return projectOutputLine({"[Image data detected and sent to Batbox]", {}},
                             imports.columns, dimPalette, true);
  std::vector<ftxui::Element> children;
  auto options = OutputLineOptions{};
  options.columns = imports.columns;
  options.verbose = card.verbose;
  options.expandShellOutput = imports.expandShellOutput;
  options.inVirtualList = imports.inVirtualList;
  if (card.stdoutText) {
    children.push_back(projectOutputLine(
        outputLineCard(*card.stdoutText, options, imports.outputLine),
        imports.columns, imports.outputPalette));
  }
  if (card.stderrText) {
    options.isError = true;
    children.push_back(projectOutputLine(
        outputLineCard(*card.stderrText, options, imports.outputLine),
        imports.columns, imports.outputPalette));
  }
  if (card.cwdResetWarning)
    children.push_back(projectOutputLine({*card.cwdResetWarning, {}},
                                         imports.columns, dimPalette));
  if (card.noOutputText) {
    std::string text = *card.noOutputText;
    if (card.backgroundHint) {
      if (!imports.backgroundShortcutHint)
        throw std::logic_error("Background Mission shortcut hint required");
      text += " " + imports.backgroundShortcutHint();
    }
    children.push_back(projectOutputLine({std::move(text), {}},
                                         imports.columns, dimPalette, true));
  }
  if (card.timeoutMs)
    children.push_back(projectOutputLine(
        {"(timeout " + shell::formatDuration(*card.timeoutMs, true) + ")", {}},
        imports.columns, dimPalette));
  return ftxui::vbox(std::move(children));
}
} // namespace batbox::interactive::cards
