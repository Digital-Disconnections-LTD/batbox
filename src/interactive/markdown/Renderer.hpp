#pragma once

#include "interactive/markdown/Format.hpp"
#include "interactive/markdown/Lexer.hpp"
#include "interactive/markdown/Table.hpp"
#include <ftxui/dom/elements.hpp>

namespace batbox::interactive::markdown {

struct StreamingState {
  std::string stablePrefix;
  void reset(){stablePrefix.clear();}
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
