#pragma once
#include "terminal_render/ScreenSearch.hpp"
#include "terminal_dom/Node.hpp"
#include "terminal_output/Output.hpp"
#include <functional>
#include <memory>

namespace batbox::terminal_render {
using ReactElementRef=std::shared_ptr<void>;
using ReconcilerContainerRef=std::shared_ptr<void>;
struct RenderToScreenResult {
  terminal_screen::ScreenRef screen;
  double height=0;
};
struct RenderToScreenImports {
  // createRoot binds dom::createNode and holds its FocusManager. This
  // remains explicit because the React renderer owns the FocusManager.
  std::function<terminal_dom::ElementRef()> createRoot;
  std::function<void(terminal_dom::DOMElement&)> attachDisabledFocusManager;
  std::function<ReconcilerContainerRef(terminal_dom::DOMElement&)>
    createLegacyContainer;
  std::function<void(const ReactElementRef&,
    const ReconcilerContainerRef&)> updateContainerSync;
  std::function<void()> flushSyncWork;
  std::function<double()> performanceNow;
  std::function<void()> resetLayoutShifted;
  std::function<void(terminal_dom::DOMElement&,
    terminal_output::Output&)> renderNodeToOutput;
  std::function<terminal_screen::ScreenRef(terminal_output::Output&)>
    flushOutput;
  std::function<void(const std::string&)> logForDebugging;
};
class RenderToScreen {
public:
  RenderToScreen(RenderToScreenImports imports,ScanState& scanState);
  RenderToScreenResult render(const ReactElementRef& element,double width);
  terminal_dom::ElementRef root()const{return root_;}
  double calls()const{return calls_;}
private:
  RenderToScreenImports imports_;
  ScanState& scanState_;
  terminal_dom::ElementRef root_;
  ReconcilerContainerRef container_;
  std::shared_ptr<terminal_screen::StylePool> stylePool_;
  std::shared_ptr<terminal_screen::CharPool> charPool_;
  std::shared_ptr<terminal_screen::HyperlinkPool> hyperlinkPool_;
  std::unique_ptr<terminal_output::Output> output_;
  double reconcileMs_=0,yogaMs_=0,paintMs_=0,calls_=0;
};
} // namespace batbox::terminal_render
