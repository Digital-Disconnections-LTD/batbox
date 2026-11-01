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
  // Optional styled header and result, matching the live Gadget card.
  std::function<std::string()> spinnerGlyph;
  ftxui::Color running=ftxui::Color::Default;
  std::string okGlyph="✓",errorGlyph="✗";
  std::function<ftxui::Element(const Json&)> renderInline;
  std::function<ftxui::Element(const Json&)> renderResult;
};
std::optional<ftxui::Element> projectVerboseGadgetUse(
  const VerboseGadgetUseCard& card,const VerboseProjectionImports& imports);
}
