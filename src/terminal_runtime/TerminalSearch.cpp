#include "terminal_runtime/TerminalControls.hpp"
#include "terminal_dom/Dom.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace batbox::terminal_runtime {
void TerminalControls::setSearchHighlight(const std::string& query){
  if(searchHighlightQuery_==query)return;
  searchHighlightQuery_=query;
  if(!interaction_||!interaction_->scheduleRender)
    throw std::logic_error("Render scheduler import required");
  interaction_->scheduleRender();
}
std::vector<terminal_render::MatchPosition>
TerminalControls::scanElementSubtree(terminal_dom::DOMElement& element){
  if(searchHighlightQuery_.empty()||!element.yogaNode)return {};
  const auto yoga=std::dynamic_pointer_cast<terminal_dom::FullLayoutNode>(
    element.yogaNode);
  if(!yoga)throw std::logic_error("Full layout node required");
  const auto width=std::ceil(yoga->getComputedWidth());
  const auto height=std::ceil(yoga->getComputedHeight());
  if(width<=0||height<=0)return {};
  if(!interaction_||!interaction_->renderNode||
     !interaction_->outputRender||!interaction_->scanState)
    throw std::logic_error("Render and scan imports required");
  const auto left=yoga->getComputedLeft();
  const auto top=yoga->getComputedTop();
  auto screen=terminal_screen::createScreen(width,height,stylePool_,
    charPool_,hyperlinkPool_,services_.screenDebug);
  terminal_output::Output output(width,height,stylePool_,screen,
    services_.screenDebug);
  terminal_render::RenderNodeOptions options;
  options.offsetX=-left;
  options.offsetY=-top;
  interaction_->renderNode->render(element,output,std::move(options));
  const auto rendered=output.get(*interaction_->outputRender);
  terminal_dom::markDirty(&element);
  const auto positions=terminal_render::scanPositions(*rendered,
    searchHighlightQuery_,*interaction_->scanState);
  std::string selected;
  for(std::size_t i=0;i<std::min<std::size_t>(10,positions.size());++i){
    if(i)selected+=",";
    selected+=value::numberToString(positions[i].row)+":"+
      value::numberToString(positions[i].col);
  }
  if(positions.size()>10)selected+=",…";
  services_.logForDebugging("scanElementSubtree: q='"+
    searchHighlightQuery_+"' el="+value::numberToString(width)+"x"+
    value::numberToString(height)+"@("+value::numberToString(left)+","+
    value::numberToString(top)+") n="+
    value::numberToString(static_cast<double>(positions.size()))+
    " ["+selected+"]","");
  return positions;
}
void TerminalControls::setSearchPositions(SearchPositionsRef state){
  searchPositions_=std::move(state);
  if(!interaction_||!interaction_->scheduleRender)
    throw std::logic_error("Render scheduler import required");
  interaction_->scheduleRender();
}
void TerminalControls::setSelectionBgColor(const std::string& color){
  if(!interaction_||!interaction_->chalk)
    throw std::logic_error("Chalk color import required");
  const auto wrapped=terminal_color::colorize(std::string(1,'\0'),color,
    terminal_color::ColorType::Background,*interaction_->chalk);
  const auto nul=wrapped.find('\0');
  if(nul==std::string::npos||nul==0||nul==wrapped.size()-1){
    stylePool_->setSelectionBg({});
    return;
  }
  stylePool_->setSelectionBg(std::make_shared<ansi_tokenize::AnsiCode>(
    ansi_tokenize::AnsiCode{"ansi",wrapped.substr(0,nul),
      wrapped.substr(nul+1)}));
}
} // namespace batbox::terminal_runtime
