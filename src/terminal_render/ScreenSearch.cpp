#include "terminal_render/ScreenSearch.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::terminal_render {
std::vector<MatchPosition> scanPositions(const terminal_screen::Screen& screen,
    const std::string& query,ScanState& state){
  const auto lq=terminal_search::lowercase(value::utf16CodeUnits(query));
  if(lq.empty())return {};
  const auto w=screen.width,h=screen.height;
  const auto& noSelect=*screen.noSelect;
  if(!state.performanceNow)
    throw std::invalid_argument("Scan performance clock import required");
  const auto t0=state.performanceNow();
  auto positions=terminal_search::scanRows(w,h,lq,[&](double col,double row){
      const auto idx=row*w+col;
      const auto cell=terminal_screen::cellAtIndex(screen,idx);
      const bool selected=std::isfinite(idx)&&idx>=0&&
        std::trunc(idx)==idx&&
        idx<static_cast<double>(noSelect.size())&&
        noSelect[static_cast<std::size_t>(idx)]==1;
      return terminal_search::Cell{cell.character,
        cell.width==terminal_screen::CellWidth::SpacerTail||
        cell.width==terminal_screen::CellWidth::SpacerHead||selected};
  });
  state.scanMs+=state.performanceNow()-t0;
  return positions;
}
bool applyPositionedHighlight(terminal_screen::Screen& screen,
    terminal_screen::StylePool& stylePool,
    const std::vector<MatchPosition>& positions,double rowOffset,
    double currentIdx){
  if(currentIdx<0||currentIdx>=static_cast<double>(positions.size()))
    return false;
  if(!std::isfinite(currentIdx)||std::trunc(currentIdx)!=currentIdx)
    throw std::runtime_error(
      "Cannot read properties of undefined (reading 'row')");
  const auto& p=positions.at(static_cast<std::size_t>(currentIdx));
  const auto row=p.row+rowOffset;
  if(row<0||row>=screen.height)return false;
  const auto rowOff=row*screen.width;
  for(double col=p.col;col<p.col+p.len;col++){
    if(col<0||col>=screen.width)continue;
    const auto cell=terminal_screen::cellAtIndex(screen,rowOff+col);
    terminal_screen::setCellStyleId(screen,col,row,
      stylePool.withCurrentMatch(cell.styleId));
  }
  return true;
}
} // namespace batbox::terminal_render
