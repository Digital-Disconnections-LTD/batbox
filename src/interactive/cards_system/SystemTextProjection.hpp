#pragma once

#include "interactive/cards_system/SystemTextRouter.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include <functional>
#include <optional>

namespace batbox::interactive::cards_system {

struct SystemPalette {
  ftxui::Color text;
  ftxui::Color subtle;
  ftxui::Color error;
  ftxui::Color selectedBackground;
};

struct SystemProjectionImports {
  SystemPalette palette;
  std::string referenceMark;
  std::string errorDot;
  std::string eventMark;
  std::function<std::optional<ftxui::Element>(SystemTextKind,
      const Json&, const SystemTextCard&, bool verbose)> renderSpecial;
};

std::optional<ftxui::Element> projectSystemText(
    const Json& message, const SystemTextCard& card, bool verbose,
    const SystemProjectionImports& imports);
std::optional<ftxui::Element> projectSystemTextMessage(
    const Json& message, bool addMargin, bool verbose, bool transcriptMode,
    const SystemProjectionImports& imports);

} // namespace batbox::interactive::cards_system
