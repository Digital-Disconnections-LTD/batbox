#include "terminal_renderer/Renderer.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace batbox::terminal_renderer {
namespace {
std::string number(double value){return value::numberToString(value);}
}
Renderer::Renderer(terminal_dom::DOMElement& node,
    std::shared_ptr<terminal_screen::StylePool> stylePool,
    terminal_dom::NodeStash& stash,RendererServices services):
    node_(node),stylePool_(std::move(stylePool)),stash_(stash),
    services_(std::move(services)){
  if(!stylePool_||!services_.resetLayoutShifted||
     !services_.resetScrollHint||!services_.resetScrollDrainNode||
     !services_.renderNodeToOutput||!services_.getScrollDrainNode||
     !services_.getScrollHint||!services_.logForDebugging||
     !services_.outputRender.line.tokenize||
     !services_.outputRender.line.styledCharsFromTokens||
     !services_.outputRender.line.getGraphemeSegmenter||
     !services_.outputRender.line.stringWidth||
     !services_.outputRender.line.reorderBidi||
     !services_.outputRender.widestLine||
     !services_.outputRender.sliceAnsi||
     !services_.outputRender.logForDebugging)
    throw std::invalid_argument("Renderer source imports required");
}
Frame Renderer::render(const RenderOptions& options){
  const auto& frontFrame=options.frontFrame;
  const auto& backFrame=options.backFrame;
  const bool isTTY=options.isTTY;
  const double terminalWidth=options.terminalWidth;
  const double terminalRows=options.terminalRows;
  const auto prevScreen=frontFrame.screen;
  const auto backScreen=backFrame.screen;
  if(!backScreen)throw std::invalid_argument("Back frame screen required");
  const auto charPool=backScreen->charPool;
  const auto hyperlinkPool=backScreen->hyperlinkPool;
  const auto layout=node_.yogaNode?
    std::dynamic_pointer_cast<terminal_dom::FullLayoutNode>(node_.yogaNode):
    terminal_dom::FullLayoutRef{};
  if(node_.yogaNode&&!layout)
    throw std::invalid_argument("Full Yoga layout node required");
  const auto computedHeight=layout?
    std::optional<double>(layout->getComputedHeight()):std::nullopt;
  const auto computedWidth=layout?
    std::optional<double>(layout->getComputedWidth()):std::nullopt;
  const bool invalidHeight=!computedHeight||
    !std::isfinite(*computedHeight)||*computedHeight<0;
  const bool invalidWidth=!computedWidth||
    !std::isfinite(*computedWidth)||*computedWidth<0;
  if(!layout||invalidHeight||invalidWidth){
    if(layout&&(invalidHeight||invalidWidth)){
      services_.logForDebugging(
        "Invalid yoga dimensions: width="+number(*computedWidth)+
        ", height="+number(*computedHeight)+
        ", childNodes="+number(node_.childNodes.size())+
        ", terminalWidth="+number(terminalWidth)+
        ", terminalRows="+number(terminalRows),"");
    }
    return {terminal_screen::createScreen(terminalWidth,0,stylePool_,
      charPool,hyperlinkPool,services_.logForDebugging),
      {terminalWidth,terminalRows},{0,0,true},{},{}};
  }
  const double width=std::floor(*computedWidth);
  const double yogaHeight=std::floor(*computedHeight);
  const double height=options.altScreen?terminalRows:yogaHeight;
  if(options.altScreen&&yogaHeight>terminalRows){
    services_.logForDebugging(
      "alt-screen: yoga height "+number(yogaHeight)+
      " > terminalRows "+number(terminalRows)+
      " — something is rendering outside <AlternateScreen>. Overflow clipped.",
      "warn");
  }
  const auto screen=backScreen?backScreen:terminal_screen::createScreen(
    width,height,stylePool_,charPool,hyperlinkPool,
    services_.logForDebugging);
  if(output_)output_->reset(width,height,screen);
  else output_=std::make_unique<terminal_output::Output>(width,height,
    stylePool_,screen,services_.logForDebugging);
  services_.resetLayoutShifted();
  services_.resetScrollHint();
  services_.resetScrollDrainNode();
  const bool absoluteRemoved=stash_.consumeAbsoluteRemovedFlag();
  services_.renderNodeToOutput(node_,*output_,
    absoluteRemoved||options.prevFrameContaminated?ScreenRef{}:prevScreen);
  const auto renderedScreen=output_->get(services_.outputRender);
  auto* drainNode=services_.getScrollDrainNode();
  if(drainNode)terminal_dom::markDirty(drainNode);
  Frame result;
  result.scrollHint.emplace(options.altScreen?services_.getScrollHint():
    std::optional<ScrollHint>{});
  result.scrollDrainPending=drainNode!=nullptr;
  result.screen=renderedScreen;
  result.viewport={terminalWidth,
    options.altScreen?terminalRows+1:terminalRows};
  result.cursor={0,
    options.altScreen?
      std::max(0.0,std::min(screen->height,terminalRows)-1):screen->height,
    !isTTY||screen->height==0};
  return result;
}
RendererRef createRenderer(terminal_dom::DOMElement& node,
    std::shared_ptr<terminal_screen::StylePool> stylePool,
    terminal_dom::NodeStash& stash,RendererServices services){
  return std::make_shared<Renderer>(node,std::move(stylePool),stash,
    std::move(services));
}
} // namespace batbox::terminal_renderer
