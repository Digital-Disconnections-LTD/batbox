#pragma once

#include "interactive/cards/BashResultCard.hpp"
#include "interactive/cards/OutputLineCard.hpp"
#include "interactive/cards/OutputLineProjection.hpp"
#include "ftxui/dom/elements.hpp"
#include <functional>

namespace batbox::interactive::cards {

struct BashResultProjectionImports {
  int columns = 80;
  bool expandShellOutput = false;
  bool inVirtualList = false;
  OutputLineImports outputLine;
  OutputLinePalette outputPalette;
  ftxui::Color dimColor;
  std::function<std::string()> backgroundShortcutHint;
};

ftxui::Element projectBashResult(const BashResultCard& card,
                                  const BashResultProjectionImports& imports);

} // namespace batbox::interactive::cards
