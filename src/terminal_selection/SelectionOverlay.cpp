#include "terminal_selection/Selection.hpp"
#include <algorithm>
#include <cmath>

namespace batbox::terminal_selection {
void applySelectionOverlay(terminal_screen::Screen& screen,
    const SelectionState& selection,terminal_screen::StylePool& stylePool){
  const auto b=selectionBounds(selection);
  if(!b)return;
  const auto& start=*b->start;const auto& end=*b->end;
  const double width=screen.width;
  for(double row=start.row;row<=end.row&&row<screen.height;row+=1){
    const double colStart=row==start.row?start.col:0;
    const double colEnd=row==end.row?std::min(end.col,width-1):width-1;
    const double rowOff=row*width;
    for(double col=colStart;col<=colEnd;col+=1){
      const double idx=rowOff+col;
      if(std::isfinite(idx)&&idx>=0&&std::trunc(idx)==idx&&
          idx<screen.noSelect->size()&&
          (*screen.noSelect)[static_cast<std::size_t>(idx)]==1)continue;
      const auto cell=terminal_screen::cellAtIndex(screen,idx);
      terminal_screen::setCellStyleId(screen,col,row,
        stylePool.withSelectionBg(cell.styleId));
    }
  }
}
} // namespace batbox::terminal_selection
