#include "terminal_runtime/TerminalControls.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::terminal_runtime {
void TerminalControls::bindInteraction(InteractionServices services){
  if(interaction_){
    if(!services.scheduleRender)
      services.scheduleRender=interaction_->scheduleRender;
    if(!services.rootNode)services.rootNode=interaction_->rootNode;
    if(!services.focusManager)
      services.focusManager=interaction_->focusManager;
  }
  interaction_=std::move(services);
}
std::string TerminalControls::copySelectionNoClear(){
  if(!terminal_selection::hasSelection(selection_))return "";
  const auto text=terminal_selection::getSelectedText(selection_,
    *frontFrame_.screen);
  if(!text.empty()){
    if(!interaction_||!interaction_->setClipboard||
       !interaction_->unhandledClipboardRejection)
      throw std::logic_error("Clipboard and rejection imports required");
    auto self=shared_from_this();
    auto pending=interaction_->setClipboard(text);
    pending.then([self](const std::string* raw,
        const std::exception_ptr& error){
      if(error){self->interaction_->unhandledClipboardRejection(error);return;}
      if(raw&&!raw->empty())self->stdout_.write(*raw);
    });
  }
  return text;
}
std::string TerminalControls::copySelection(){
  if(!terminal_selection::hasSelection(selection_))return "";
  auto text=copySelectionNoClear();
  terminal_selection::clearSelection(selection_);
  notifySelectionChange();
  return text;
}
void TerminalControls::clearTextSelection(){
  if(!terminal_selection::hasSelection(selection_))return;
  terminal_selection::clearSelection(selection_);
  notifySelectionChange();
}
void TerminalControls::captureScrolledRows(double firstRow,double lastRow,
    bool above){
  terminal_selection::captureScrolledRows(selection_,*frontFrame_.screen,
    firstRow,lastRow,above);
}
void TerminalControls::shiftSelectionForScroll(double dRow,double minRow,
    double maxRow){
  const auto hadSel=terminal_selection::hasSelection(selection_);
  terminal_selection::shiftSelection(selection_,dRow,minRow,maxRow,
    frontFrame_.screen->width);
  if(hadSel&&!terminal_selection::hasSelection(selection_))
    notifySelectionChange();
}
void TerminalControls::moveSelectionFocus(terminal_selection::FocusMove move){
  if(!altScreenActive_)return;
  auto focus=selection_.focus;
  if(!focus)return;
  const auto width=frontFrame_.screen->width;
  const auto height=frontFrame_.screen->height;
  const auto maxCol=width-1,maxRow=height-1;
  auto col=focus->col,row=focus->row;
  switch(move){
    case terminal_selection::FocusMove::Left:
      if(col>0)--col;
      else if(row>0){col=maxCol;--row;}
      break;
    case terminal_selection::FocusMove::Right:
      if(col<maxCol)++col;
      else if(row<maxRow){col=0;++row;}
      break;
    case terminal_selection::FocusMove::Up:
      if(row>0)--row;
      break;
    case terminal_selection::FocusMove::Down:
      if(row<maxRow)++row;
      break;
    case terminal_selection::FocusMove::LineStart:col=0;break;
    case terminal_selection::FocusMove::LineEnd:col=maxCol;break;
  }
  if(col==focus->col&&row==focus->row)return;
  terminal_selection::moveFocus(selection_,col,row);
  notifySelectionChange();
}
bool TerminalControls::hasTextSelection()const{
  return terminal_selection::hasSelection(selection_);
}
std::function<void()> TerminalControls::subscribeToSelectionChange(
    SelectionListenerRef listener){
  if(!listener||!listener->callback)
    throw std::invalid_argument("Selection listener required");
  for(const auto& entry:selectionListeners_)
    if(entry->active&&entry->listener==listener)
      return [entry]{entry->active=false;};
  auto entry=std::make_shared<ListenerEntry>(ListenerEntry{listener,true});
  selectionListeners_.push_back(entry);
  return [entry]{
    entry->active=false;
  };
}
void TerminalControls::notifySelectionChange(){
  invokeRender();
  for(std::size_t i=0;i<selectionListeners_.size();++i){
    auto entry=selectionListeners_[i];
    if(entry->active)entry->listener->callback();
  }
}
void TerminalControls::handleMultiClick(double col,double row,int count){
  if(!altScreenActive_)return;
  auto& screen=*frontFrame_.screen;
  terminal_selection::startSelection(selection_,col,row);
  if(count==2)terminal_selection::selectWordAt(selection_,screen,col,row);
  else terminal_selection::selectLineAt(selection_,screen,row);
  if(!selection_.focus)selection_.focus=selection_.anchor;
  notifySelectionChange();
}
void TerminalControls::handleSelectionDrag(double col,double row){
  if(!altScreenActive_)return;
  if(selection_.anchorSpan)
    terminal_selection::extendSelection(selection_,*frontFrame_.screen,
      col,row);
  else terminal_selection::updateSelection(selection_,col,row);
  notifySelectionChange();
}
} // namespace batbox::terminal_runtime
