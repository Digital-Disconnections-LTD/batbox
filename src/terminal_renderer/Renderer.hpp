#pragma once
#include "terminal_dom/Dom.hpp"
#include "terminal_dom/FullLayoutPort.hpp"
#include "terminal_dom/NodeStash.hpp"
#include "terminal_output/OutputRender.hpp"
#include "terminal_renderer/Frame.hpp"

namespace batbox::terminal_renderer {
struct RenderOptions {
  Frame frontFrame,backFrame;
  bool isTTY=false;
  double terminalWidth=0,terminalRows=0;
  bool altScreen=false,prevFrameContaminated=false;
};
struct RendererServices {
  // The node renderer supplies these required callbacks.
  // None has a substitute default.
  std::function<void()> resetLayoutShifted,resetScrollHint,
    resetScrollDrainNode;
  std::function<void(terminal_dom::DOMElement&,terminal_output::Output&,
    ScreenRef)> renderNodeToOutput;
  std::function<terminal_dom::DOMElement*()> getScrollDrainNode;
  std::function<std::optional<ScrollHint>()> getScrollHint;
  terminal_output::OutputRenderServices outputRender;
  std::function<void(const std::string&,const std::string&)> logForDebugging;
};
class Renderer {
public:
  Renderer(terminal_dom::DOMElement& node,
    std::shared_ptr<terminal_screen::StylePool> stylePool,
    terminal_dom::NodeStash& stash,RendererServices services);
  Frame render(const RenderOptions& options);
private:
  terminal_dom::DOMElement& node_;
  std::shared_ptr<terminal_screen::StylePool> stylePool_;
  terminal_dom::NodeStash& stash_;
  RendererServices services_;
  std::unique_ptr<terminal_output::Output> output_;
};
using RendererRef=std::shared_ptr<Renderer>;
RendererRef createRenderer(terminal_dom::DOMElement& node,
  std::shared_ptr<terminal_screen::StylePool> stylePool,
  terminal_dom::NodeStash& stash,RendererServices services);
} // namespace batbox::terminal_renderer
