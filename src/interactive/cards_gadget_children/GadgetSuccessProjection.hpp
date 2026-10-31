#pragma once

#include "interactive/cards_gadget_children/GadgetSuccessCard.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include <functional>
#include <optional>

namespace batbox::interactive::cards_gadget_children {

struct SuccessProjectionImports {
  int columns = 80;
  ftxui::Color subtle;
  ftxui::Color success;
  std::string tickGlyph;
  std::function<ftxui::Element(const Json&)> renderGadgetMessage;
};

std::optional<ftxui::Element> projectGadgetSuccess(
    const GadgetSuccessCard& card,const SuccessProjectionImports& imports);

} // namespace batbox::interactive::cards_gadget_children
