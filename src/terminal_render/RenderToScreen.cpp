#include "terminal_render/RenderToScreen.hpp"
#include "terminal_dom/FullLayoutPort.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace batbox::terminal_render {
namespace {
std::string fixed(double value,int digits){
  std::ostringstream out;out<<std::fixed<<std::setprecision(digits)<<value;
  return out.str();
}
template<class F> void required(const F& callback,const char* name){
  if(!callback)throw std::invalid_argument(name);
}
}
RenderToScreen::RenderToScreen(RenderToScreenImports imports,
    ScanState& scanState):imports_(std::move(imports)),scanState_(scanState){}
RenderToScreenResult RenderToScreen::render(
    const ReactElementRef& element,double width){
  if(!root_){
    required(imports_.createRoot,"Search root import required");
    root_=imports_.createRoot();
    if(!root_)throw std::invalid_argument("Search root import returned null");
    required(imports_.attachDisabledFocusManager,
      "Search FocusManager import required");
    imports_.attachDisabledFocusManager(*root_);
    stylePool_=std::make_shared<terminal_screen::StylePool>();
    charPool_=std::make_shared<terminal_screen::CharPool>();
    hyperlinkPool_=std::make_shared<terminal_screen::HyperlinkPool>();
    required(imports_.createLegacyContainer,
      "Search reconciler createContainer import required");
    container_=imports_.createLegacyContainer(*root_);
  }
  required(imports_.performanceNow,"Search performance clock import required");
  const auto t0=imports_.performanceNow();
  required(imports_.updateContainerSync,
    "Search reconciler updateContainerSync import required");
  imports_.updateContainerSync(element,container_);
  required(imports_.flushSyncWork,
    "Search reconciler flushSyncWork import required");
  imports_.flushSyncWork();
  const auto t1=imports_.performanceNow();

  auto yoga=std::dynamic_pointer_cast<terminal_dom::FullLayoutNode>(
    root_->yogaNode);
  if(root_->yogaNode&&!yoga)
    throw std::invalid_argument("Search full Yoga layout import required");
  if(yoga){yoga->setWidth(width);yoga->calculateLayout(width);}
  const auto height=std::ceil(yoga?yoga->getComputedHeight():0);
  const auto t2=imports_.performanceNow();

  const auto screen=terminal_screen::createScreen(width,
    terminal_screen::typed_array::jsMax(1,height),stylePool_,charPool_,
    hyperlinkPool_,{});
  if(!output_){
    output_=std::make_unique<terminal_output::Output>(width,height,
      stylePool_,screen,terminal_screen::DebugWarning{});
  }else output_->reset(width,height,screen);
  required(imports_.resetLayoutShifted,
    "Search resetLayoutShifted import required");
  imports_.resetLayoutShifted();
  required(imports_.renderNodeToOutput,
    "Search renderNodeToOutput import required");
  imports_.renderNodeToOutput(*root_,*output_);
  required(imports_.flushOutput,"Search Output.get import required");
  const auto rendered=imports_.flushOutput(*output_);
  const auto t3=imports_.performanceNow();

  imports_.updateContainerSync({},container_);
  imports_.flushSyncWork();

  reconcileMs_+=t1-t0;
  yogaMs_+=t2-t1;
  paintMs_+=t3-t2;
  ++calls_;
  if(std::fmod(calls_,20)==0){
    const auto total=reconcileMs_+yogaMs_+paintMs_+scanState_.scanMs;
    required(imports_.logForDebugging,
      "Search local diagnostic import required");
    imports_.logForDebugging("renderToScreen: "+
      std::to_string(static_cast<std::int64_t>(calls_))+" calls · "+
      "reconcile="+fixed(reconcileMs_,1)+"ms yoga="+
      fixed(yogaMs_,1)+"ms paint="+fixed(paintMs_,1)+
      "ms scan="+fixed(scanState_.scanMs,1)+"ms · total="+
      fixed(total,1)+"ms · avg "+fixed(total/calls_,2)+"ms/call");
  }
  return {rendered,height};
}
} // namespace batbox::terminal_render
