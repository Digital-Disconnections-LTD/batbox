#pragma once

#include "interactive/cards_collapsed/VerboseGadgetUse.hpp"
#include <ftxui/dom/elements.hpp>

namespace batbox::interactive::cards_collapsed {
struct VerboseProjectionImports {
  std::function<ftxui::Element(const Json&)> renderComponent;
  std::function<ftxui::Color(const Json&)> resolveColor;
  std::function<bool(bool)> isBlinking;
  ftxui::Color subtle=ftxui::Color::Default;
  ftxui::Color success=ftxui::Color::Default;
  ftxui::Color error=ftxui::Color::Default;
  std::string blackCircle;
};
std::optional<ftxui::Element> projectVerboseGadgetUse(
  const VerboseGadgetUseCard& card,const VerboseProjectionImports& imports);
}
