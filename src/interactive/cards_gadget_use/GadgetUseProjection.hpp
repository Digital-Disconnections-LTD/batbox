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
  // Optional styled header. With a spinner provider, running Gadgets animate
  // and settled ones show okGlyph or errorGlyph; the use text follows the
  // name on the same row without parentheses.
  std::function<std::string()> spinnerGlyph;
  ftxui::Color running=ftxui::Color::Default;
  ftxui::Color nameColor=ftxui::Color::Default;
  std::string okGlyph="✓",errorGlyph="✗";
  std::function<ftxui::Element(const Json&)> renderInline;
  std::function<ftxui::Element(const Json&)> renderTag;
  std::function<ftxui::Element(const Json&)> renderProgress;
};

std::optional<ftxui::Element> projectGadgetUse(const GadgetUseCard& card,
                                             const UseProjectionImports& imports);
}
