#pragma once
#include "terminal_screen/Screen.hpp"
#include "terminal_search/Scan.hpp"
#include <functional>
#include <string>
#include <vector>

namespace batbox::terminal_render {
struct ScanState {
  double scanMs=0;
  std::function<double()> performanceNow;
};
std::vector<MatchPosition> scanPositions(const terminal_screen::Screen& screen,
  const std::string& query,ScanState& state);
bool applyPositionedHighlight(terminal_screen::Screen& screen,
  terminal_screen::StylePool& stylePool,
  const std::vector<MatchPosition>& positions,double rowOffset,
  double currentIdx);
} // namespace batbox::terminal_render
