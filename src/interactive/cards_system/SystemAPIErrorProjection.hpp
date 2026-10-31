#pragma once

#include "interactive/cards_system/SystemAPIErrorCard.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include <functional>
#include <optional>

namespace batbox::interactive::cards_system {

struct APIErrorPalette {
  ftxui::Color error;
  ftxui::Color subtle;
};

std::optional<ftxui::Element> projectSystemAPIError(
    const APIErrorCard& card, const APIErrorPalette& palette,
    const std::function<ftxui::Element()>& expandHint);

} // namespace batbox::interactive::cards_system
