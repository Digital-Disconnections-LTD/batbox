#include "terminal_runtime/TerminalControls.hpp"
#include "terminal_runtime/SearchHighlight.hpp"
#include "terminal_optimizer/Optimizer.hpp"
#include "terminal_log_update/PatchJson.hpp"
#include "terminal_dom/Dom.hpp"
#include "termio/Csi.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace batbox::terminal_runtime {
namespace {
double orDefault(std::optional<double> value,double fallback){
  return value&&*value!=0&&!std::isnan(*value)?*value:fallback;
}
std::string reasonName(terminal_renderer::FlickerReason reason){
  using terminal_renderer::FlickerReason;
  if(reason==FlickerReason::Resize)return "resize";
  if(reason==FlickerReason::Offscreen)return "offscreen";
  return "clear";
}
}
void TerminalControls::bindRenderCycle(RenderCycleServices services){
  if(!services.flushInteractionTime||!services.performanceNow||
     !services.renderer||!services.renderState||!services.log||
     !services.profile||!services.debugRepaints||!services_.ownerLoop)
    throw std::invalid_argument("Render cycle imports required");
  renderCycle_=std::move(services);
  services_.resetLog=[log=renderCycle_->log]{log->reset();};
  services_.onRender=[this]{onRender();};
}
void TerminalControls::onRender(){
  if(isUnmounted_||isPaused_)return;
  if(!renderCycle_)
    throw std::logic_error("Render cycle not bound");
  if(drainTimer_.valid()){
    drainTimer_.cancel();
    drainTimer_={};
  }
  auto& cycle=*renderCycle_;
  cycle.flushInteractionTime();
  const auto renderStart=cycle.performanceNow();
  const auto terminalWidth=orDefault(stdout_.columns(),80);
  const auto terminalRows=orDefault(stdout_.rows(),24);
  auto frame=cycle.renderer->render(terminal_renderer::RenderOptions{
    frontFrame_,backFrame_,stdout_.isTTY(),terminalWidth,terminalRows,
    altScreenActive_,prevFrameContaminated_});
  const auto rendererMs=cycle.performanceNow()-renderStart;

  const auto follow=cycle.renderState->consumeFollowScroll();
  if(follow&&selection_.anchor&&
     selection_.anchor->row>=follow->viewportTop&&
     selection_.anchor->row<=follow->viewportBottom){
    const auto delta=follow->delta;
    const auto top=follow->viewportTop;
    const auto bottom=follow->viewportBottom;
    if(selection_.isDragging){
      if(terminal_selection::hasSelection(selection_))
        terminal_selection::captureScrolledRows(selection_,
          *frontFrame_.screen,top,top+delta-1,true);
      terminal_selection::shiftAnchor(selection_,-delta,top,bottom);
    }else if(!selection_.focus||
        (selection_.focus->row>=top&&selection_.focus->row<=bottom)){
      if(terminal_selection::hasSelection(selection_))
        terminal_selection::captureScrolledRows(selection_,
          *frontFrame_.screen,top,top+delta-1,true);
      const auto cleared=terminal_selection::shiftSelectionForFollow(
        selection_,-delta,top,bottom);
      if(cleared){
        for(std::size_t i=0;i<selectionListeners_.size();++i){
          auto entry=selectionListeners_[i];
          if(entry->active)entry->listener->callback();
        }
      }
    }
  }

  bool selActive=false,hlActive=false;
  if(altScreenActive_){
    selActive=terminal_selection::hasSelection(selection_);
    if(selActive)terminal_selection::applySelectionOverlay(*frame.screen,
      selection_,*stylePool_);
    hlActive=applySearchHighlight(*frame.screen,searchHighlightQuery_,
      *stylePool_);
    if(searchPositions_){
      const auto applied=terminal_render::applyPositionedHighlight(
        *frame.screen,*stylePool_,searchPositions_->positions,
        searchPositions_->rowOffset,searchPositions_->currentIdx);
      hlActive=hlActive||applied;
    }
  }
  if(cycle.renderState->didLayoutShift()||selActive||hlActive||
     prevFrameContaminated_){
    frame.screen->damage=std::make_shared<terminal_screen::Rectangle>(
      terminal_screen::Rectangle{0,0,frame.screen->width,
        frame.screen->height});
  }

  auto prevFrame=frontFrame_;
  if(altScreenActive_)prevFrame.cursor={0,0,false};
  const auto tDiff=cycle.performanceNow();
  auto diff=std::make_shared<terminal_renderer::Diff>(cycle.log->render(
    prevFrame,frame,altScreenActive_,cycle.syncOutputSupported));
  const auto diffMs=cycle.performanceNow()-tDiff;
  backFrame_=frontFrame_;
  frontFrame_=frame;
  if(renderStart-lastPoolResetTime_>5*60*1000){
    resetPools();
    lastPoolResetTime_=renderStart;
  }

  std::vector<terminal_renderer::Flicker> flickers;
  for(const auto& patch:*diff){
    const auto* clear=std::get_if<terminal_renderer::ClearTerminalPatch>(
      &patch);
    if(!clear)continue;
    flickers.push_back({frame.screen->height,frame.viewport.height,
      clear->reason});
    if(cycle.debugRepaints->enabled()&&clear->debug){
      if(!interaction_||!interaction_->rootNode)
        throw std::logic_error("DOM root import required");
      const auto chain=terminal_dom::findOwnerChainAtRow(
        *interaction_->rootNode,clear->debug->triggerY);
      std::string owners;
      for(const auto& owner:chain){
        if(!owners.empty())owners+=" < ";
        owners+=owner;
      }
      if(owners.empty())owners="(no owner chain captured)";
      services_.logForDebugging("[REPAINT] full reset · "+
        reasonName(clear->reason)+" · row "+
        value::numberToString(clear->debug->triggerY)+"\n"+
        "  prev: \""+clear->debug->prevLine+"\"\n"+
        "  next: \""+clear->debug->nextLine+"\"\n"+
        "  culprit: "+owners,"warn");
    }
  }
  const auto tOptimize=cycle.performanceNow();
  auto optimized=terminal_optimizer::optimize(diff);
  const auto optimizeMs=cycle.performanceNow()-tOptimize;
  const bool hasDiff=!optimized->empty();
  if(altScreenActive_&&hasDiff){
    if(needsEraseBeforePaint_){
      needsEraseBeforePaint_=false;
      optimized->insert(optimized->begin(),terminal_renderer::StdoutPatch{
        termio::ERASE_SCREEN+termio::CURSOR_HOME});
    }else optimized->insert(optimized->begin(),
      terminal_renderer::StdoutPatch{termio::CURSOR_HOME});
    optimized->push_back(terminal_renderer::StdoutPatch{
      altScreenParkPatch_});
  }

  std::optional<CursorPoint> target;
  if(cursorDeclaration_){
    if(!interaction_||!interaction_->nodeStash)
      throw std::logic_error("Node cache import required");
    const auto rect=interaction_->nodeStash->get(
      *cursorDeclaration_->node);
    if(rect)target=CursorPoint{
      rect->x+cursorDeclaration_->relativeX,
      rect->y+cursorDeclaration_->relativeY};
  }
  const auto parked=displayCursor_;
  const bool targetMoved=target&&(!parked||parked->x!=target->x||
    parked->y!=target->y);
  if(hasDiff||targetMoved||(!target&&parked)){
    if(parked&&!altScreenActive_&&hasDiff){
      const auto dx=prevFrame.cursor.x-parked->x;
      const auto dy=prevFrame.cursor.y-parked->y;
      if(dx!=0||dy!=0)
        optimized->insert(optimized->begin(),
          terminal_renderer::StdoutPatch{termio::cursorMove(dx,dy)});
    }
    if(target){
      if(altScreenActive_){
        const auto row=std::min(std::max(target->y+1,1.0),terminalRows);
        const auto col=std::min(std::max(target->x+1,1.0),terminalWidth);
        optimized->push_back(terminal_renderer::StdoutPatch{
          termio::cursorPosition(row,col)});
      }else{
        const CursorPoint from=!hasDiff&&parked?*parked:
          CursorPoint{frame.cursor.x,frame.cursor.y};
        const auto dx=target->x-from.x,dy=target->y-from.y;
        if(dx!=0||dy!=0)optimized->push_back(
          terminal_renderer::StdoutPatch{termio::cursorMove(dx,dy)});
      }
      displayCursor_=target;
    }else{
      if(parked&&!altScreenActive_&&!hasDiff){
        const auto dx=frame.cursor.x-parked->x;
        const auto dy=frame.cursor.y-parked->y;
        if(dx!=0||dy!=0)optimized->push_back(
          terminal_renderer::StdoutPatch{termio::cursorMove(dx,dy)});
      }
      displayCursor_.reset();
    }
  }
  const auto tWrite=cycle.performanceNow();
  terminal_log_update::writeDiffToTerminal(
    [this](const std::string& data){stdout_.write(data);},
    *optimized,altScreenActive_&&!cycle.syncOutputSupported,cycle.output);
  const auto writeMs=cycle.performanceNow()-tWrite;
  prevFrameContaminated_=selActive||hlActive;
  if(frame.scrollDrainPending.value_or(false)){
    auto self=shared_from_this();
    drainTimer_=services_.ownerLoop->setTimeout(16>>2,
      [self]{self->onRender();});
  }
  const auto yogaMs=cycle.profile->getLastYogaMs();
  const auto commitMs=cycle.profile->getLastCommitMs();
  const auto yc=lastYogaCounters_;
  cycle.profile->resetProfileCounters();
  lastYogaCounters_={};
  if(cycle.onFrame){
    terminal_renderer::FramePhases phases;
    phases.renderer=rendererMs;
    phases.diff=diffMs;
    phases.optimize=optimizeMs;
    phases.write=writeMs;
    phases.patches=diff->size();
    phases.yoga=yogaMs;
    phases.commit=commitMs;
    phases.yogaVisited=yc.visited;
    phases.yogaMeasured=yc.measured;
    phases.yogaStashHits=yc.stashHits;
    phases.yogaLive=yc.live;
    cycle.onFrame(terminal_renderer::FrameEvent{
      cycle.performanceNow()-renderStart,phases,std::move(flickers)});
  }
}
} // namespace batbox::terminal_runtime
