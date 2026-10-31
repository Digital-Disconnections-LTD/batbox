#include "terminal_runtime/TerminalControls.hpp"
#include "termio/Csi.hpp"
#include "termio/Dec.hpp"
#include <cmath>

namespace batbox::terminal_runtime {
namespace {
double orDefault(std::optional<double> value,double fallback){
  return value&&*value!=0&&!std::isnan(*value)?*value:fallback;
}
}
void TerminalControls::handleResume(){
  if(!stdout_.isTTY())return;
  if(altScreenActive_){reenterAltScreen();return;}
  frontFrame_=terminal_renderer::emptyFrame(frontFrame_.viewport.height,
    frontFrame_.viewport.width,stylePool_,charPool_,hyperlinkPool_,
    services_.screenDebug);
  backFrame_=terminal_renderer::emptyFrame(backFrame_.viewport.height,
    backFrame_.viewport.width,stylePool_,charPool_,hyperlinkPool_,
    services_.screenDebug);
  services_.resetLog();
  displayCursor_.reset();
}
void TerminalControls::handleResize(){
  const auto cols=orDefault(stdout_.columns(),80);
  const auto rows=orDefault(stdout_.rows(),24);
  if(cols==terminalColumns_&&rows==terminalRows_)return;
  terminalColumns_=cols;
  terminalRows_=rows;
  altScreenParkPatch_=termio::cursorPosition(terminalRows_,1);
  if(altScreenActive_&&!isPaused_&&stdout_.isTTY()){
    if(altScreenMouseTracking_)
      stdout_.write(termio::ENABLE_MOUSE_TRACKING);
    resetFramesForAltScreen();
    needsEraseBeforePaint_=true;
  }
  if(currentNode_)services_.renderCurrentNode(currentNode_);
}
void TerminalControls::reassertTerminalModes(bool includeAltScreen){
  if(!stdout_.isTTY()||isPaused_)return;
  if(services_.supportsExtendedKeys())
    stdout_.write(termio::DISABLE_KITTY_KEYBOARD+
      termio::ENABLE_KITTY_KEYBOARD+termio::ENABLE_MODIFY_OTHER_KEYS);
  if(!altScreenActive_)return;
  if(altScreenMouseTracking_)
    stdout_.write(termio::ENABLE_MOUSE_TRACKING);
  if(includeAltScreen)reenterAltScreen();
}
void TerminalControls::enterAlternateScreen(){
  pause();
  suspendStdin();
  stdout_.write(termio::DISABLE_KITTY_KEYBOARD+
    termio::DISABLE_MODIFY_OTHER_KEYS+
    (altScreenMouseTracking_?termio::DISABLE_MOUSE_TRACKING:"")+
    (altScreenActive_?"":termio::ENTER_ALT_SCREEN)+
    termio::DFE+"\x1b[0m"+termio::SHOW_CURSOR+
    termio::ERASE_SCREEN+termio::CURSOR_HOME);
}
void TerminalControls::exitAlternateScreen(){
  stdout_.write((altScreenActive_?termio::ENTER_ALT_SCREEN:"")+
    termio::ERASE_SCREEN+termio::CURSOR_HOME+
    (altScreenMouseTracking_?termio::ENABLE_MOUSE_TRACKING:"")+
    (altScreenActive_?"":termio::EXIT_ALT_SCREEN)+
    termio::HIDE_CURSOR);
  resumeStdin();
  if(altScreenActive_)resetFramesForAltScreen();
  else repaint();
  resume();
  stdout_.write(termio::EFE+
    (services_.supportsExtendedKeys()?
      termio::DISABLE_KITTY_KEYBOARD+
      termio::ENABLE_KITTY_KEYBOARD+
      termio::ENABLE_MODIFY_OTHER_KEYS:""));
}
void TerminalControls::reenterAltScreen(){
  stdout_.write(termio::ENTER_ALT_SCREEN+termio::ERASE_SCREEN+
    termio::CURSOR_HOME+
    (altScreenMouseTracking_?termio::ENABLE_MOUSE_TRACKING:""));
  resetFramesForAltScreen();
}
void TerminalControls::resetFramesForAltScreen(){
  const auto rows=terminalRows_,cols=terminalColumns_;
  const auto blank=[&](){
    terminal_renderer::Frame result;
    result.screen=terminal_screen::createScreen(cols,rows,stylePool_,
      charPool_,hyperlinkPool_,services_.screenDebug);
    result.viewport={cols,rows+1};
    result.cursor={0,0,true};
    return result;
  };
  frontFrame_=blank();
  backFrame_=blank();
  services_.resetLog();
  displayCursor_.reset();
  prevFrameContaminated_=true;
}
} // namespace batbox::terminal_runtime
