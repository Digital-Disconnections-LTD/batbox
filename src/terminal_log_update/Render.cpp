#include "terminal_log_update/Internal.hpp"
#include "runtime/JsonValue.hpp"
#include "termio/Csi.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::terminal_log_update {
terminal_renderer::Diff LogUpdate::render(const Frame& prev,const Frame& next,
    bool altScreen,bool decstbmSafe){
  if(!options_.isTTY)return renderFullFrame(next);
  if(!services_.performanceNow)
    throw std::invalid_argument("LogUpdate performance clock import required");
  const double startTime=services_.performanceNow();
  auto& stylePool=*options_.stylePool;
  if(next.viewport.height<prev.viewport.height||
      (prev.viewport.width!=0&&
        next.viewport.width!=prev.viewport.width))
    return fullResetSequence(next,FlickerReason::Resize,stylePool);

  Diff scrollPatch;
  if(altScreen&&next.scrollHint&&*next.scrollHint&&decstbmSafe){
    const auto& hint=**next.scrollHint;
    if(hint.top>=0&&hint.bottom<prev.screen->height&&
        hint.bottom<next.screen->height){
      terminal_screen::shiftRows(*prev.screen,hint.top,hint.bottom,hint.delta);
      scrollPatch.push_back(terminal_renderer::StdoutPatch{
        termio::setScrollRegion(hint.top+1,hint.bottom+1)+
        (hint.delta>0?termio::scrollUp(hint.delta):
          termio::scrollDown(-hint.delta))+
        termio::RESET_SCROLL_REGION+termio::CURSOR_HOME});
    }
  }

  const bool cursorAtBottom=prev.cursor.y>=prev.screen->height;
  const bool isGrowing=next.screen->height>prev.screen->height;
  const bool prevHadScrollback=cursorAtBottom&&
    prev.screen->height>=prev.viewport.height;
  const bool isShrinking=next.screen->height<prev.screen->height;
  const bool nextFitsViewport=next.screen->height<=prev.viewport.height;
  if(prevHadScrollback&&nextFitsViewport&&isShrinking){
    if(!services_.logForDebugging)
      throw std::invalid_argument("LogUpdate debug import required");
    services_.logForDebugging("Full reset (shrink->below): prevHeight="+
      value::numberToString(prev.screen->height)+", nextHeight="+
      value::numberToString(next.screen->height)+", viewport="+
      value::numberToString(prev.viewport.height));
    return fullResetSequence(next,FlickerReason::Offscreen,stylePool);
  }
  if(prev.screen->height>=prev.viewport.height&&
      prev.screen->height>0&&cursorAtBottom&&!isGrowing){
    const double viewportY=prev.screen->height-prev.viewport.height;
    const double scrollbackRows=viewportY+1;
    double scrollbackChangeY=-1;
    terminal_screen::diffEach(*prev.screen,*next.screen,
      [&](double,double y,const Cell*,const Cell*){
        if(y<scrollbackRows){scrollbackChangeY=y;return true;}
        return false;
      });
    if(scrollbackChangeY>=0)
      return fullResetSequence(next,FlickerReason::Offscreen,stylePool,
        terminal_renderer::ClearTerminalPatch::Debug{scrollbackChangeY,
          readLine(*prev.screen,scrollbackChangeY),
          readLine(*next.screen,scrollbackChangeY)});
  }

  VirtualScreen screen({prev.cursor.x,prev.cursor.y},next.viewport.width);
  const double heightDelta=std::max(next.screen->height,1.0)-
    std::max(prev.screen->height,1.0);
  const bool shrinking=heightDelta<0,growing=heightDelta>0;
  if(shrinking){
    const double linesToClear=prev.screen->height-next.screen->height;
    if(linesToClear>prev.viewport.height)
      return fullResetSequence(next,FlickerReason::Offscreen,stylePool);
    screen.txn([linesToClear](const terminal_screen::Point& p){
      return std::pair<Diff,Delta>{
        {terminal_renderer::ClearPatch{linesToClear},
          terminal_renderer::CursorMovePatch{0,-1}},
        {-p.x,-linesToClear}};
    });
  }
  const double cursorRestoreScroll=prevHadScrollback?1:0;
  const double viewportY=growing?
    std::max(0.0,prev.screen->height-prev.viewport.height+
      cursorRestoreScroll):
    std::max(prev.screen->height,next.screen->height)-
      next.viewport.height+cursorRestoreScroll;
  std::int32_t currentStyleId=stylePool.none();
  std::optional<std::string> currentHyperlink;
  bool needsFullReset=false;
  double resetTriggerY=-1;
  terminal_screen::diffEach(*prev.screen,*next.screen,
    [&](double x,double y,const Cell* removed,const Cell* added){
      if(growing&&y>=prev.screen->height)return false;
      if(added&&(added->width==terminal_screen::CellWidth::SpacerTail||
          added->width==terminal_screen::CellWidth::SpacerHead))return false;
      if(removed&&(removed->width==terminal_screen::CellWidth::SpacerTail||
          removed->width==terminal_screen::CellWidth::SpacerHead)&&!added)
        return false;
      if(added&&terminal_screen::isEmptyCellAt(*next.screen,x,y)&&!removed)
        return false;
      if(y<viewportY){
        needsFullReset=true;resetTriggerY=y;return true;
      }
      moveCursorTo(screen,x,y);
      if(added){
        currentHyperlink=transitionHyperlink(screen.diff,currentHyperlink,
          added->hyperlink);
        const auto styleStr=stylePool.transition(currentStyleId,
          added->styleId);
        if(writeCellWithStyleStr(screen,*added,styleStr))
          currentStyleId=added->styleId;
      }else if(removed){
        const auto styleIdToReset=currentStyleId;
        const auto hyperlinkToReset=currentHyperlink;
        currentStyleId=stylePool.none();
        currentHyperlink.reset();
        screen.txn([&](const terminal_screen::Point&){
          Diff patches;
          transitionStyle(patches,stylePool,styleIdToReset,
            stylePool.none());
          transitionHyperlink(patches,hyperlinkToReset,{});
          patches.push_back(terminal_renderer::StdoutPatch{" "});
          return std::pair<Diff,Delta>{std::move(patches),{1,0}};
        });
      }
      return false;
    });
  if(needsFullReset)
    return fullResetSequence(next,FlickerReason::Offscreen,stylePool,
      terminal_renderer::ClearTerminalPatch::Debug{resetTriggerY,
        readLine(*prev.screen,resetTriggerY),
        readLine(*next.screen,resetTriggerY)});

  currentStyleId=transitionStyle(screen.diff,stylePool,currentStyleId,
    stylePool.none());
  currentHyperlink=transitionHyperlink(screen.diff,currentHyperlink,{});
  if(growing)
    renderFrameSlice(screen,next,prev.screen->height,next.screen->height,
      stylePool);
  if(!altScreen&&next.cursor.y>=next.screen->height){
    screen.txn([&](const terminal_screen::Point& p){
      const double rowsToCreate=next.cursor.y-p.y;
      if(rowsToCreate>0)
        return std::pair<Diff,Delta>{crlfPatches(rowsToCreate),
          {-p.x,rowsToCreate}};
      const double dy=next.cursor.y-p.y;
      if(dy!=0||p.x!=next.cursor.x)
        return std::pair<Diff,Delta>{
          {terminal_renderer::CarriageReturnPatch{},
            terminal_renderer::CursorMovePatch{next.cursor.x,dy}},
          {next.cursor.x-p.x,dy}};
      return std::pair<Diff,Delta>{{},{0,0}};
    });
  }else if(!altScreen){
    moveCursorTo(screen,next.cursor.x,next.cursor.y);
  }

  const double elapsed=services_.performanceNow()-startTime;
  if(elapsed>50){
    if(!services_.logForDebugging)
      throw std::invalid_argument("LogUpdate debug import required");
    const auto& damage=next.screen->damage;
    const auto damageInfo=damage?
      value::numberToString(damage->width)+"x"+
      value::numberToString(damage->height)+" at ("+
      value::numberToString(damage->x)+","+
      value::numberToString(damage->y)+")":"none";
    services_.logForDebugging("Slow render: "+fixedOne(elapsed)+
      "ms, screen: "+value::numberToString(next.screen->height)+"x"+
      value::numberToString(next.screen->width)+", damage: "+damageInfo+
      ", changes: "+value::numberToString(screen.diff.size()));
  }
  if(!scrollPatch.empty()){
    scrollPatch.insert(scrollPatch.end(),screen.diff.begin(),
      screen.diff.end());
    return scrollPatch;
  }
  return screen.diff;
}
} // namespace batbox::terminal_log_update
