#include "terminal_runtime/TerminalControls.hpp"
#include <stdexcept>

namespace batbox::terminal_runtime {
bool TerminalControls::dispatchClick(double col,double row){
  if(!altScreenActive_)return false;
  if(!interaction_||!interaction_->rootNode||!interaction_->nodeStash||
     !interaction_->pointerHandlers)
    throw std::logic_error("DOM hit-test imports required");
  const auto blank=terminal_screen::isEmptyCellAt(*frontFrame_.screen,col,row);
  return terminal_hit_test::dispatchClick(*interaction_->rootNode,col,row,
    blank,*interaction_->nodeStash,*interaction_->pointerHandlers);
}
void TerminalControls::dispatchHover(double col,double row){
  if(!altScreenActive_)return;
  if(!interaction_||!interaction_->rootNode||!interaction_->nodeStash||
     !interaction_->pointerHandlers)
    throw std::logic_error("DOM hit-test imports required");
  terminal_hit_test::dispatchHover(*interaction_->rootNode,col,row,
    hoveredNodes_,*interaction_->nodeStash,*interaction_->pointerHandlers);
}
void TerminalControls::dispatchKeyboardEvent(
    const terminal_events::ParsedKey& parsedKey){
  if(!interaction_||!interaction_->rootNode||!interaction_->focusManager||
     !interaction_->dispatcher||!interaction_->monotonicNow)
    throw std::logic_error("Keyboard dispatch imports required");
  auto target=interaction_->focusManager->activeElement;
  if(!target)target=interaction_->rootNode;
  terminal_events::KeyboardEvent event(parsedKey,interaction_->monotonicNow);
  interaction_->dispatcher->dispatchDiscrete(*target,event);
  if(!event.defaultPrevented()&&parsedKey.name=="tab"&&
     !parsedKey.ctrl&&!parsedKey.meta){
    if(parsedKey.shift)
      interaction_->focusManager->focusPrevious(*interaction_->rootNode);
    else interaction_->focusManager->focusNext(*interaction_->rootNode);
  }
}
std::optional<std::string> TerminalControls::getHyperlinkAt(
    double col,double row)const{
  if(!altScreenActive_)return {};
  const auto& screen=*frontFrame_.screen;
  const auto cell=terminal_screen::cellAt(screen,col,row);
  auto url=cell?cell->hyperlink:std::optional<std::string>{};
  if((!url||url->empty())&&cell&&
     cell->width==terminal_screen::CellWidth::SpacerTail&&col>0){
    const auto head=terminal_screen::cellAt(screen,col-1,row);
    url=head?head->hyperlink:std::optional<std::string>{};
  }
  if(url)return url;
  return terminal_selection::findPlainTextUrlAt(screen,col,row);
}
void TerminalControls::setHyperlinkClick(
    std::function<void(const std::string&)> callback){
  onHyperlinkClick_=std::move(callback);
}
void TerminalControls::openHyperlink(const std::string& url){
  if(onHyperlinkClick_)onHyperlinkClick_(url);
}
void TerminalControls::setCursorDeclaration(
    CursorDeclarationRef declaration,
    std::optional<terminal_dom::ElementRef> clearIfNode){
  if(!declaration&&clearIfNode&&
     (!cursorDeclaration_||cursorDeclaration_->node!=*clearIfNode))
    return;
  cursorDeclaration_=std::move(declaration);
}
} // namespace batbox::terminal_runtime
