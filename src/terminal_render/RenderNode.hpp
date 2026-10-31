#pragma once
#include "terminal_render/RenderState.hpp"
#include "terminal_render/StyledText.hpp"
#include "terminal_render/Border.hpp"
#include "terminal_output/Output.hpp"
#include "terminal_dom/FullLayoutPort.hpp"

namespace batbox::terminal_render {
struct RenderNodeOptions {
  double offsetX=0,offsetY=0;
  terminal_screen::ScreenRef prevScreen;
  bool skipSelfBlit=false;
  std::optional<std::string> inheritedBackgroundColor;
};
struct RenderNodeImports {
  StyledTextImports text;
  BorderImports border;
  std::function<double(const std::string&)> widestLine;
  std::function<std::optional<std::string>()> termProgram;
  std::function<bool()> isXtermJs;
};
double getMaxWidth(const terminal_dom::FullLayoutNode& yogaNode);
class RenderNodeRenderer {
public:
  RenderNodeRenderer(RenderState& state,RenderNodeImports imports);
  void render(terminal_dom::DOMElement& node,terminal_output::Output& output,
    RenderNodeOptions options);
private:
  void renderText(terminal_dom::DOMElement& node,
    terminal_output::Output& output,
    const terminal_dom::FullLayoutNode& yoga,double x,double y,
    const std::optional<std::string>& inheritedBackgroundColor);
  void renderBox(terminal_dom::DOMElement& node,
    terminal_output::Output& output,
    const terminal_dom::FullLayoutNode& yoga,double x,double y,
    double width,double height,bool hasRemovedChild,
    const terminal_screen::ScreenRef& prevScreen,bool positionChanged,
    const terminal_dom::StashedRef& stashed,
    const std::optional<std::string>& inheritedBackgroundColor);
  void renderScrollBox(terminal_dom::DOMElement& node,
    terminal_output::Output& output,
    const terminal_dom::FullLayoutNode& yoga,double x,double y,
    double width,double height,std::optional<double> y1,
    std::optional<double> y2,bool hasRemovedChild,
    const terminal_screen::ScreenRef& prevScreen,bool positionChanged,
    const terminal_dom::StashedRef& stashed,
    const std::optional<std::string>& boxBackgroundColor);
  void renderScrollFast(terminal_dom::DOMElement& content,
    terminal_output::Output& output,double x,double y,double width,
    double contentX,double contentY,double scrollTop,double innerHeight,
    double padTop,std::optional<double> y1,
    const terminal_screen::ScreenRef& prevScreen,bool hasRemovedChild,
    const std::optional<std::string>& boxBackgroundColor,
    const ScrollHint& hint);
  void renderChildren(terminal_dom::DOMElement& node,
    terminal_output::Output& output,double offsetX,double offsetY,
    bool hasRemovedChild,const terminal_screen::ScreenRef& prevScreen,
    const std::optional<std::string>& inheritedBackgroundColor);
  bool clipsBothAxes(const terminal_dom::DOMElement& node)const;
  bool siblingSharesY(const terminal_dom::DOMElement& node,
    const terminal_dom::FullLayoutNode& yogaNode)const;
  void blitEscapingAbsoluteDescendants(terminal_dom::DOMElement& node,
    terminal_output::Output& output,
    const terminal_screen::ScreenRef& prevScreen,double px,double py,
    double pw,double ph);
  void renderScrolledChildren(terminal_dom::DOMElement& node,
    terminal_output::Output& output,double offsetX,double offsetY,
    bool hasRemovedChild,const terminal_screen::ScreenRef& prevScreen,
    double scrollTopY,double scrollBottomY,
    const std::optional<std::string>& inheritedBackgroundColor,
    bool preserveCulledStash=false);
  void dropSubtreeStash(terminal_dom::DOMElement& node);
  RenderState& state_;
  RenderNodeImports imports_;
};
} // namespace batbox::terminal_render
