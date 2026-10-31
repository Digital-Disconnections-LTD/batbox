#include "terminal_runtime/TerminalControls.hpp"
#include "termio/Csi.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::terminal_runtime {
namespace {
double orDefault(std::optional<double> value,double fallback){
  return value&&*value!=0&&!std::isnan(*value)?*value:fallback;
}
}
TerminalControls::TerminalControls(OutputPort& stdout,InputPort& stdin,
    ControlServices services):stdout_(stdout),stdin_(stdin),
    services_(std::move(services)){
  if(!services_.supportsExtendedKeys||!services_.logForDebugging||
     !services_.screenDebug)
    throw std::invalid_argument("Terminal control imports required");
  if(!services_.drain.windows&&
     !services_.drain.openNonblockingTty&&
     !services_.drain.readTty&&
     !services_.drain.closeTty)
    services_.drain=indigenousDrainServices();
  if(!services_.drain.windows&&
     (!services_.drain.openNonblockingTty||
      !services_.drain.readTty||!services_.drain.closeTty))
    throw std::invalid_argument("Complete TTY drain imports required");
}
TerminalControls::TerminalControls(OutputPort& stdout,InputPort& stdin,
    std::shared_ptr<terminal_screen::StylePool> stylePool,
    std::shared_ptr<terminal_screen::CharPool> charPool,
    std::shared_ptr<terminal_screen::HyperlinkPool> hyperlinkPool,
    ControlServices services):
    TerminalControls(stdout,stdin,std::move(services)){
  if(!stylePool||!charPool||!hyperlinkPool||
     !services_.flushSyncFromReconciler||!services_.resetLog||
     !services_.renderCurrentNode)
    throw std::invalid_argument("Terminal screen pools required");
  stylePool_=std::move(stylePool);
  charPool_=std::move(charPool);
  hyperlinkPool_=std::move(hyperlinkPool);
  terminalColumns_=orDefault(stdout_.columns(),80);
  terminalRows_=orDefault(stdout_.rows(),24);
  altScreenParkPatch_=termio::cursorPosition(terminalRows_,1);
  frontFrame_=terminal_renderer::emptyFrame(terminalRows_,terminalColumns_,
    stylePool_,charPool_,hyperlinkPool_,services_.screenDebug);
  backFrame_=terminal_renderer::emptyFrame(terminalRows_,terminalColumns_,
    stylePool_,charPool_,hyperlinkPool_,services_.screenDebug);
}
void TerminalControls::initializeCore(){
  if(stylePool_||charPool_||hyperlinkPool_)
    throw std::logic_error("Terminal core already initialized");
  terminalColumns_=orDefault(stdout_.columns(),80);
  terminalRows_=orDefault(stdout_.rows(),24);
  altScreenParkPatch_=termio::cursorPosition(terminalRows_,1);
  stylePool_=std::make_shared<terminal_screen::StylePool>();
  charPool_=std::make_shared<terminal_screen::CharPool>();
  hyperlinkPool_=std::make_shared<terminal_screen::HyperlinkPool>();
  frontFrame_=terminal_renderer::emptyFrame(terminalRows_,terminalColumns_,
    stylePool_,charPool_,hyperlinkPool_,services_.screenDebug);
  backFrame_=terminal_renderer::emptyFrame(terminalRows_,terminalColumns_,
    stylePool_,charPool_,hyperlinkPool_,services_.screenDebug);
}
void TerminalControls::pause(){
  services_.flushSyncFromReconciler();
  invokeRender();
  isPaused_=true;
}
void TerminalControls::resume(){
  isPaused_=false;
  invokeRender();
}
void TerminalControls::repaint(){
  frontFrame_=terminal_renderer::emptyFrame(frontFrame_.viewport.height,
    frontFrame_.viewport.width,stylePool_,charPool_,hyperlinkPool_,
    services_.screenDebug);
  backFrame_=terminal_renderer::emptyFrame(backFrame_.viewport.height,
    backFrame_.viewport.width,stylePool_,charPool_,hyperlinkPool_,
    services_.screenDebug);
  services_.resetLog();
  displayCursor_.reset();
}
void TerminalControls::forceRedraw(){
  if(!stdout_.isTTY()||isUnmounted_||isPaused_)return;
  stdout_.write(termio::ERASE_SCREEN+termio::CURSOR_HOME);
  if(altScreenActive_)resetFramesForAltScreen();
  else{
    repaint();
    prevFrameContaminated_=true;
  }
  invokeRender();
}
void TerminalControls::invokeRender(){
  if(!services_.onRender)
    throw std::logic_error("Render cycle import required");
  services_.onRender();
}
void TerminalControls::invalidatePrevFrame(){prevFrameContaminated_=true;}
void TerminalControls::setAltScreenActive(bool active,bool mouseTracking){
  if(altScreenActive_==active)return;
  altScreenActive_=active;
  altScreenMouseTracking_=active&&mouseTracking;
  if(active)resetFramesForAltScreen();
  else repaint();
}
void TerminalControls::writeRaw(const std::string& data){stdout_.write(data);}
void TerminalControls::resetLineCount(){
  if(!stdout_.isTTY())return;
  backFrame_=frontFrame_;
  frontFrame_=terminal_renderer::emptyFrame(frontFrame_.viewport.height,
    frontFrame_.viewport.width,stylePool_,charPool_,hyperlinkPool_,
    services_.screenDebug);
  services_.resetLog();
  displayCursor_.reset();
}
void TerminalControls::resetPools(){
  charPool_=std::make_shared<terminal_screen::CharPool>();
  hyperlinkPool_=std::make_shared<terminal_screen::HyperlinkPool>();
  terminal_screen::migrateScreenPools(*frontFrame_.screen,
    charPool_,hyperlinkPool_);
  backFrame_.screen->charPool=charPool_;
  backFrame_.screen->hyperlinkPool=hyperlinkPool_;
}
} // namespace batbox::terminal_runtime
