#include "terminal_log_update/Internal.hpp"
#include "ansi_tokenize/AnsiTokenize.hpp"
#include <stdexcept>

namespace batbox::terminal_log_update {
LogUpdate::LogUpdate(Options options,Services services):
  options_(std::move(options)),services_(std::move(services)),
  previousOutput_(""){
  if(!options_.stylePool)
    throw std::invalid_argument("LogUpdate StylePool import required");
  if(!services_.oscModule)
    throw std::invalid_argument("LogUpdate OSC module import required");
}
terminal_renderer::Diff LogUpdate::renderPreviousOutput_DEPRECATED(
    const Frame& prevFrame){
  if(!options_.isTTY)
    return {terminal_renderer::StdoutPatch{"\n"}};
  return getRenderOpsForDone(prevFrame);
}
void LogUpdate::reset(){previousOutput_="";}
terminal_renderer::Diff LogUpdate::renderFullFrame(const Frame& frame){
  const auto& screen=*frame.screen;
  std::vector<std::string> lines;
  auto currentStyles=std::make_shared<ansi_tokenize::Codes>();
  std::optional<std::string> currentHyperlink;
  for(double y=0;y<screen.height;y+=1){
    std::string line;
    for(double x=0;x<screen.width;x+=1){
      const auto cell=terminal_screen::cellAt(screen,x,y);
      if(cell&&cell->width!=terminal_screen::CellWidth::SpacerTail){
        if(cell->hyperlink!=currentHyperlink){
          if(currentHyperlink)line+=services_.oscModule->linkEnd();
          if(cell->hyperlink)
            line+=services_.oscModule->link(*cell->hyperlink);
          currentHyperlink=cell->hyperlink;
        }
        const auto cellStyles=options_.stylePool->get(cell->styleId);
        const auto styleDiff=ansi_tokenize::diffAnsiCodes(*currentStyles,
          *cellStyles);
        if(!styleDiff.empty()){
          line+=ansi_tokenize::ansiCodesToString(styleDiff);
          currentStyles=cellStyles;
        }
        line+=cell->character;
      }
    }
    if(currentHyperlink){
      line+=services_.oscModule->linkEnd();
      currentHyperlink.reset();
    }
    const auto reset=ansi_tokenize::diffAnsiCodes(*currentStyles,{});
    if(!reset.empty()){
      line+=ansi_tokenize::ansiCodesToString(reset);
      currentStyles=std::make_shared<ansi_tokenize::Codes>();
    }
    lines.push_back(trimEndJs(line));
  }
  if(lines.empty())return {};
  std::string text;
  for(std::size_t i=0;i<lines.size();++i){
    if(i)text+='\n';text+=lines[i];
  }
  return {terminal_renderer::StdoutPatch{std::move(text)}};
}
terminal_renderer::Diff LogUpdate::getRenderOpsForDone(const Frame& prev){
  previousOutput_="";
  if(!prev.cursor.visible)return {terminal_renderer::CursorShowPatch{}};
  return {};
}
} // namespace batbox::terminal_log_update
