#pragma once

#include "interactive/cards_collapsed/CollapsedPlan.hpp"
#include <ftxui/dom/elements.hpp>

namespace batbox::interactive::cards_collapsed {
struct CollapsedProjectionImports {
  std::function<ftxui::Color(const Json&)> resolveColor;
  std::function<bool(bool)> isBlinking;
  std::function<ftxui::Element(const Json&)> renderVerboseGadgetUse;
  std::function<ftxui::Element(const std::string&)> renderAnsi;
  std::function<std::string(const std::string&)> basename;
  std::function<std::string(double)> formatSecondsShort;
  ftxui::Color subtle=ftxui::Color::Default;
  ftxui::Color success=ftxui::Color::Default;
  ftxui::Color error=ftxui::Color::Default;
  std::string blackCircle;
  // Optional: animate active groups and mark settled ones like Gadget cards.
  std::function<std::string()> spinnerGlyph;
  ftxui::Color running=ftxui::Color::Default;
  ftxui::Color text=ftxui::Color::Default;
  std::string okGlyph="✓";
};
std::optional<ftxui::Element> projectCollapsedReadSearch(
    const CollapsedCard& card,const CollapsedProjectionImports& imports);
}
