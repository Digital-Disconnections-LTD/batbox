#include "terminal_log_update/Internal.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::terminal_log_update {
Diff crlfPatches(double rows){
  const double length=1+rows;
  if(!std::isfinite(length)||length<0||std::trunc(length)!=length||
      length>4294967295.0)
    throw std::range_error("Invalid array length");
  Diff patches;
  patches.reserve(static_cast<std::size_t>(length));
  patches.push_back(terminal_renderer::CarriageReturnPatch{});
  for(double i=0;i<rows;i+=1)
    patches.push_back(terminal_renderer::StdoutPatch{"\n"});
  return patches;
}
VirtualScreen& renderFrameSlice(VirtualScreen& screen,const Frame& frame,
    double startY,double endY,StylePool& styles){
  std::int32_t currentStyleId=styles.none();
  std::optional<std::string> currentHyperlink;
  std::int32_t lastRenderedStyleId=-1;
  const auto& source=*frame.screen;
  const double screenWidth=source.width;
  double index=startY*screenWidth;
  for(double y=startY;y<endY;y+=1){
    if(screen.cursor.y<y){
      const double rowsToAdvance=y-screen.cursor.y;
      screen.txn([&](const terminal_screen::Point& prev){
        return std::pair<Diff,Delta>{crlfPatches(rowsToAdvance),
          {-prev.x,rowsToAdvance}};
      });
    }
    lastRenderedStyleId=-1;
    for(double x=0;x<screenWidth;x+=1,index+=1){
      const auto cell=terminal_screen::visibleCellAtIndex(*source.cells,
        *source.charPool,*source.hyperlinkPool,index,lastRenderedStyleId);
      if(!cell)continue;
      moveCursorTo(screen,x,y);
      currentHyperlink=transitionHyperlink(screen.diff,currentHyperlink,
        cell->hyperlink);
      const auto styleStr=styles.transition(currentStyleId,cell->styleId);
      if(writeCellWithStyleStr(screen,*cell,styleStr)){
        currentStyleId=cell->styleId;
        lastRenderedStyleId=cell->styleId;
      }
    }
    currentStyleId=transitionStyle(screen.diff,styles,currentStyleId,
      styles.none());
    currentHyperlink=transitionHyperlink(screen.diff,currentHyperlink,{});
    screen.txn([](const terminal_screen::Point& prev){
      return std::pair<Diff,Delta>{
        {terminal_renderer::CarriageReturnPatch{},
          terminal_renderer::StdoutPatch{"\n"}},
        {-prev.x,1}};
    });
  }
  transitionStyle(screen.diff,styles,currentStyleId,styles.none());
  transitionHyperlink(screen.diff,currentHyperlink,{});
  return screen;
}
} // namespace batbox::terminal_log_update
