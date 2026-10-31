#pragma once

#include "interactive/cards_gadget_result/FallbackErrorCard.hpp"
#include "interactive/cards_gadget_result/GadgetErrorCard.hpp"
#include "interactive/cards/OutputLineProjection.hpp"
#include <optional>

namespace batbox::interactive::cards_gadget_result {

struct GadgetErrorPalette {
  cards::OutputLinePalette output;
  ftxui::Color subtle;
};

ftxui::Element projectCanceledGadgetResult(ftxui::Color subtle);
std::optional<ftxui::Element> projectSimpleGadgetError(
    const GadgetErrorCard& card, ftxui::Color subtle);
ftxui::Element projectFallbackError(const FallbackErrorCard& card,
                                    int columns,
                                    const GadgetErrorPalette& palette);

} // namespace batbox::interactive::cards_gadget_result
