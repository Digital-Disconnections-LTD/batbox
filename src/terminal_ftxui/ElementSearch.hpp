#pragma once
#include "text_sdk/scan/Scan.hpp"
#include <ftxui/dom/node.hpp>
#include <optional>

namespace batbox::terminal_ftxui {
struct PositionedMatches {
  std::vector<text_sdk::scan::MatchPosition> positions;
  double rowOffset=0,currentIndex=0;
};
std::vector<text_sdk::scan::MatchPosition> scanElement(
  const ftxui::Element& element,int columns,int rows,const std::string& query);
// Every match is inverted; the current one also takes the highlight color.
ftxui::Element withSearchOverlay(ftxui::Element content,
  std::function<std::string()> query,
  std::function<std::optional<PositionedMatches>()> current,
  ftxui::Color highlight=ftxui::Color::Yellow);
} // namespace batbox::terminal_ftxui
