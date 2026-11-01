#pragma once

#include "interactive/markdown/Format.hpp"
#include "interactive/markdown/Lexer.hpp"
#include "interactive/markdown/Table.hpp"
#include <ftxui/dom/elements.hpp>

namespace batbox::interactive::markdown {

struct StreamingState {
  std::string stablePrefix;
  // The rendered stable prefix, reused while its text, width and dimming
  // are unchanged so each frame renders only the growing suffix.
  std::string renderedPrefix;
  ftxui::Element prefixElement;
  int prefixColumns=-1;
  bool prefixDim=false;
  void reset(){
    stablePrefix.clear();renderedPrefix.clear();prefixElement=nullptr;
    prefixColumns=-1;prefixDim=false;
  }
};

class Renderer {
public:
  Renderer(FormatServices services,WrapAnsi wrap,int terminalColumns);
  ftxui::Element render(const std::string& content,bool dim=false);
  ftxui::Element renderStreaming(StreamingState& state,
      const std::string& content,bool dim=false);
  void setTerminalColumns(int columns){columns_=columns;}
  TokenStash& stash(){return stash_;}
private:
  FormatServices services_;
  WrapAnsi wrap_;
  int columns_;
  TokenStash stash_;
};

} // namespace batbox::interactive::markdown
