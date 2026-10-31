#pragma once

#include "interactive/cards_remaining/MessageLeaves.hpp"
#include <ftxui/dom/elements.hpp>

namespace batbox::interactive::cards_remaining {
struct LeafPalette {
  ftxui::Color text=ftxui::Color::Default;
  ftxui::Color subtle=ftxui::Color::Default;
  ftxui::Color success=ftxui::Color::Default;
  ftxui::Color error=ftxui::Color::Default;
  std::string blackCircle;
  std::string successTick;
  std::function<bool(bool)> isBlinking;
};
ftxui::Element projectUserImage(const ImageCard& card,
                                const LeafPalette& palette);
ftxui::Element projectCompactSummary(const CompactCard& card,
                                     const LeafPalette& palette);
ftxui::Element projectCompactBoundary(const std::string& text,
                                      const LeafPalette& palette);
std::optional<ftxui::Element> projectAdvisor(const AdvisorCard& card,
                                             const LeafPalette& palette);
std::optional<ftxui::Element> projectPartnerHeader(
    const PartnerHeaderCard& card,
    const std::function<ftxui::Color(const std::string&)>& toColor,
    const LeafPalette& palette);
}
