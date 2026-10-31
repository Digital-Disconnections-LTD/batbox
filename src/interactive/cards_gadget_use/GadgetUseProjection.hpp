#pragma once

#include "interactive/cards_gadget_use/GadgetUseDecision.hpp"
#include <ftxui/dom/elements.hpp>

namespace batbox::interactive::cards_gadget_use {

struct UseProjectionImports {
  std::function<ftxui::Element(const Json&)> renderGadgetMessage;
  std::function<ftxui::Color(const Json&)> resolveColor;
  std::function<bool(bool)> isBlinking;
  ftxui::Color subtle=ftxui::Color::Default;
  ftxui::Color success=ftxui::Color::Default;
  ftxui::Color error=ftxui::Color::Default;
  std::string blackCircle;
};

std::optional<ftxui::Element> projectGadgetUse(const GadgetUseCard& card,
                                             const UseProjectionImports& imports);
}
