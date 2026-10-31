#pragma once
#include "terminal_search/Scan.hpp"
#include <ftxui/dom/node.hpp>
#include <optional>

namespace batbox::terminal_ftxui {
struct PositionedMatches {
  std::vector<terminal_render::MatchPosition> positions;
  double rowOffset=0,currentIndex=0;
};
std::vector<terminal_render::MatchPosition> scanElement(
  const ftxui::Element& element,int columns,int rows,const std::string& query);
ftxui::Element withSearchOverlay(ftxui::Element content,
  std::function<std::string()> query,
  std::function<std::optional<PositionedMatches>()> current);
} // namespace batbox::terminal_ftxui
