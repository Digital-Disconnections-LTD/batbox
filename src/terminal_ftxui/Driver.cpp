#include "terminal_ftxui/Driver.hpp"
#include "terminal_ftxui/IndigenousStringMetrics.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <utility>
#if defined(_WIN32)
#include <cstdio>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace batbox::terminal_ftxui {
class Driver::RootComponent final:public ftxui::ComponentBase {
public:
  explicit RootComponent(Driver& owner):owner_(owner){}
  ftxui::Element OnRender()override{
    auto element=owner_.callbacks_.render();
    if(!element)throw std::logic_error("FTXUI render element required");
    owner_.frameRendered_=true;
    return element;
  }
  bool OnEvent(ftxui::Event event)override{
    if(owner_.callbacks_.onEvent)owner_.callbacks_.onEvent(event);
    // Keys belong to the raw parser. Unclaimed pointer gestures belong to
    // FTXUI's selection engine; custom redraw events preserve the highlight.
    if(event==ftxui::Event::Custom)return false;
    if(event.is_mouse()){
      if(owner_.callbacks_.consumeMouseEvent&&
          owner_.callbacks_.consumeMouseEvent(event)){
        owner_.selectionDragging_=false;return true;
      }
      const auto& mouse=event.mouse();
      if(mouse.button==ftxui::Mouse::Left){
        if(mouse.motion==ftxui::Mouse::Pressed){
          owner_.selectionDragging_=true;owner_.selectionMoved_=false;
        }else if(mouse.motion==ftxui::Mouse::Moved&&owner_.selectionDragging_)
          owner_.selectionMoved_=true;
        else if(mouse.motion==ftxui::Mouse::Released&&owner_.selectionDragging_){
          owner_.copySelectionPending_=owner_.selectionMoved_;
          owner_.selectionDragging_=false;
        }
      }
      return false;
    }
    return true;
  }
private:
  Driver& owner_;
};
Driver::Driver(event_loop::Loop& owner,Options options,Callbacks callbacks):
    owner_(owner),options_(options),callbacks_(std::move(callbacks)){
  if(!owner_.isOwnerThread())
    throw std::logic_error("FTXUI driver requires event loop owner");
  if(!callbacks_.render||!callbacks_.onRawInputBytes||
      options_.pumpIntervalMs<=0)
    throw std::invalid_argument("FTXUI driver imports required");
  if(options_.redirectedWidth<=0||options_.redirectedHeight<=0)
    throw std::invalid_argument("Redirected terminal dimensions required");
}
Driver::~Driver(){
  if(running_)teardown(false);
}
void Driver::start(){
  if(!owner_.isOwnerThread())
    throw std::logic_error("FTXUI start requires owner thread");
  if(running_)throw std::logic_error("FTXUI driver already started");
  if(suspended_)throw std::logic_error("FTXUI driver is suspended");
  exitNotified_=false;
#if defined(_WIN32)
  interactive_=options_.interactive&&options_.inputFd==_fileno(stdin)&&
    options_.outputFd==_fileno(stdout)&&
    _isatty(options_.inputFd)!=0&&_isatty(options_.outputFd)!=0;
#else
  interactive_=options_.interactive&&options_.inputFd==STDIN_FILENO&&
    options_.outputFd==STDOUT_FILENO&&
    ::isatty(options_.inputFd)==1&&::isatty(options_.outputFd)==1;
#endif
  if(interactive_)startInteractive();
  else startRedirected();
}
void Driver::startInteractive(){
  if(!callbacks_.onResize)
    throw std::invalid_argument("Interactive resize callback required");
  static const auto metrics=[] {
    auto intl=std::make_shared<intl::Stash>(intl::indigenousServices());
    auto width=std::make_shared<text_width::StringWidth>(
      text_width::indigenousServices(intl));
    return std::pair{std::move(intl),std::move(width)};
  }();
  installIndigenousStringMetrics(metrics.first,metrics.second);
  app_=std::make_unique<ftxui::App>(options_.mode==Mode::Fullscreen?
    ftxui::App::FullscreenAlternateScreen():
    ftxui::App::TerminalOutput());
  app_->TrackMouse(options_.mode==Mode::Fullscreen);
  app_->HandlePipedInput(false);
  app_->ForceHandleCtrlC(false);
  app_->ForceHandleCtrlZ(false);
  auto weak=weak_from_this();
  if(weak.expired())
    throw std::logic_error("FTXUI driver requires shared lifetime");
  app_->SetRawInputCallback([weak](std::string_view bytes){
    if(const auto owner=weak.lock()){
      if(!owner->owner_.isOwnerThread())
        throw std::logic_error("FTXUI input arrived off owner thread");
      owner->callbacks_.onRawInputBytes(bytes);
    }
  });
  component_=std::make_shared<RootComponent>(*this);
  try{
    loop_=std::make_unique<ftxui::Loop>(app_.get(),component_);
    running_=true;
    checkResize();
    app_->Post(ftxui::Event::Custom);
    pumpOnce();
    if(running_)flushScrollback();
    if(running_)schedulePump();
  }catch(...){teardown(false);throw;}
}
void Driver::startRedirected(){
  if(!callbacks_.renderPlain||!callbacks_.writePlain)
    throw std::invalid_argument("Plain redirected output callbacks required");
  running_=true;
  try{
    lastSize_=ftxui::Dimensions{
      options_.redirectedWidth,options_.redirectedHeight};
    if(callbacks_.onResize)callbacks_.onResize(*lastSize_);
    drawRedirected();
  }catch(...){teardown(false);throw;}
}
void Driver::drawRedirected(){
  if(!running_)return;
  if(inPump_){redrawAfterPump_=true;return;}
  inPump_=true;
  try{
    const auto text=callbacks_.renderPlain();
    if(!text.empty())callbacks_.writePlain(text);
    inPump_=false;
  }catch(...){inPump_=false;throw;}
  if(redrawAfterPump_){
    redrawAfterPump_=false;
    auto weak=weak_from_this();
    owner_.queueNextTurn([weak]{
      if(const auto self=weak.lock())self->drawRedirected();});
  }
}
void Driver::schedulePump(){
  if(!running_||!interactive_)return;
  auto weak=weak_from_this();
  pumpTimer_=owner_.setTimeout(options_.pumpIntervalMs,[weak]{
    if(const auto self=weak.lock()){
      self->pumpTimer_={};
      self->pumpOnce();
      if(self->running_)self->schedulePump();
    }
  });
}
void Driver::checkResize(){
  if(!interactive_)return;
  const auto current=ftxui::Terminal::Size();
  if(!lastSize_||current.dimx!=lastSize_->dimx||
      current.dimy!=lastSize_->dimy){
    lastSize_=current;
    callbacks_.onResize(current);
    if(app_)app_->Post(ftxui::Event::Custom);
  }
}
void Driver::pumpOnce(){
  if(!owner_.isOwnerThread())
    throw std::logic_error("FTXUI pump requires owner thread");
  if(!running_||!interactive_||inPump_)return;
  inPump_=true;
  try{
    loop_->RunOnce();
    if(frameRendered_){
      frameRendered_=false;
      if(callbacks_.onFrameCommitted)callbacks_.onFrameCommitted();
      if(copySelectionPending_){
        copySelectionPending_=false;
        const auto selected=selectedText();
        if(!selected.empty()&&callbacks_.copySelectedText)
          callbacks_.copySelectedText(selected);
      }
    }
    flushRaw();
    inPump_=false;
    if(pendingMode_){
      const auto mode=*pendingMode_;pendingMode_.reset();
      teardown(false);options_.mode=mode;start();return;
    }
    if(!running_)return;
    checkResize();
    if(loop_->HasQuitted()){teardown(true);return;}
    flushScrollback();
    if(redrawAfterPump_){redrawAfterPump_=false;requestRedraw();}
  }catch(...){
    inPump_=false;teardown(true);throw;
  }
}
void Driver::writeRaw(std::string_view bytes){
  if(!owner_.isOwnerThread())
    throw std::logic_error("FTXUI raw output requires owner thread");
  if(bytes.empty()||!running_||!interactive_)return;
  pendingRaw_.emplace_back(bytes);
  if(!inPump_)flushRaw();
}
void Driver::flushRaw(){
  if(pendingRaw_.empty()||!running_||!interactive_)return;
  // FTXUI sends frames through std::cout. Its flush completed before this
  // owner-thread callback, so the OSC sequence cannot split a frame.
  for(const auto& sequence:pendingRaw_)
    std::cout.write(sequence.data(),static_cast<std::streamsize>(sequence.size()));
  std::cout.flush();
  if(!std::cout)throw std::runtime_error("Terminal control output failed");
  pendingRaw_.clear();
}
void Driver::commitScrollback(const std::vector<CommittedRow>& rows){
  if(!owner_.isOwnerThread())
    throw std::logic_error("FTXUI scrollback commit requires owner thread");
  if(options_.mode!=Mode::Inline||!options_.interactive)return;
  for(const auto& row:rows){
    if(row.id.empty()||row.text.find_first_of("\r\n")!=std::string::npos)
      throw std::invalid_argument("Committed row requires ID and one display line");
    if(committedRowIds_.contains(row.id)||pendingRowIds_.contains(row.id))
      continue;
    pendingRowIds_.insert(row.id);
    pendingRows_.push_back(row);
  }
  if(pendingRows_.empty()||!running_||!interactive_)return;
  if(inPump_){redrawAfterPump_=true;return;}
  flushScrollback();
}
void Driver::flushScrollback(){
  if(pendingRows_.empty()||!running_||!interactive_||
     options_.mode!=Mode::Inline)return;
  std::string text;
  for(const auto& row:pendingRows_){
    text+=row.text;
    text+="\r\n";
  }
  app_->CommitScrollback(text);
  for(const auto& row:pendingRows_)committedRowIds_.insert(row.id);
  pendingRows_.clear();
  pendingRowIds_.clear();
  requestRedraw();
}
void Driver::requestRedraw(){
  if(!owner_.isOwnerThread())
    throw std::logic_error("FTXUI redraw requires owner thread");
  if(!running_)return;
  if(!interactive_){drawRedirected();return;}
  if(inPump_){redrawAfterPump_=true;return;}
  app_->Post(ftxui::Event::Custom);
}
std::string Driver::selectedText()const{
  return app_?app_->GetSelection():std::string{};
}
void Driver::withRestoredIO(const std::function<void()>& callback){
  if(!owner_.isOwnerThread())
    throw std::logic_error("FTXUI restored I/O requires owner thread");
  if(!callback)throw std::invalid_argument("Restored I/O callback required");
  if(!running_||!interactive_){callback();return;}
  std::exception_ptr error;
  auto restored=app_->WithRestoredIO([&]{
    try{callback();}catch(...){error=std::current_exception();}
  });
  restored();
  requestRedraw();
  if(error)std::rethrow_exception(error);
}
void Driver::suspend(){
  if(!owner_.isOwnerThread())
    throw std::logic_error("FTXUI suspend requires owner thread");
  if(suspended_)return;
  if(!running_)throw std::logic_error("FTXUI driver is not running");
  if(inPump_)throw std::logic_error("FTXUI suspend during input dispatch");
  teardown(false);
  suspended_=true;
}
void Driver::resume(){
  if(!owner_.isOwnerThread())
    throw std::logic_error("FTXUI resume requires owner thread");
  if(!suspended_)return;
  suspended_=false;
  try{start();}catch(...){suspended_=true;throw;}
}
void Driver::setMode(Mode mode){
  if(!owner_.isOwnerThread())
    throw std::logic_error("FTXUI mode change requires owner thread");
  if(options_.mode==mode)return;
  if(!running_){options_.mode=mode;return;}
  if(inPump_){pendingMode_=mode;return;}
  teardown(false);options_.mode=mode;start();
}
void Driver::teardown(bool notifyExit){
  pumpTimer_.cancel();pumpTimer_={};
  running_=false;
  loop_.reset();
  component_.reset();
  app_.reset();
  lastSize_.reset();
  pendingMode_.reset();
  redrawAfterPump_=false;
  frameRendered_=false;
  selectionDragging_=selectionMoved_=copySelectionPending_=false;
  pendingRaw_.clear();
  if(notifyExit&&!exitNotified_){
    exitNotified_=true;
    if(callbacks_.onExit)callbacks_.onExit();
  }
}
void Driver::stop(){
  if(!owner_.isOwnerThread())
    throw std::logic_error("FTXUI stop requires owner thread");
  if(suspended_){
    suspended_=false;
    if(!exitNotified_){
      exitNotified_=true;
      if(callbacks_.onExit)callbacks_.onExit();
    }
    return;
  }
  if(!running_)return;
  if(inPump_){
    auto weak=weak_from_this();
    owner_.queueNextTurn([weak]{
      if(const auto self=weak.lock())self->stop();});
    return;
  }
  teardown(true);
}
} // namespace batbox::terminal_ftxui
