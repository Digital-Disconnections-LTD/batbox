#pragma once

#include "interactive/markdown/AnsiProjection.hpp"
#include "interactive/markdown/Format.hpp"
#include "interactive/markdown/Lexer.hpp"
#include "interactive/markdown/Table.hpp"
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <vector>

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

// A live answer or reasoning block drawn as a column of pieces, each one row
// of the transcript list. Stacking the pieces, with a blank row where `gap`
// is set, draws exactly the element renderStreaming or render builds; only
// the rows near the viewport are laid out, and settled pieces are reused.
struct MarkdownPiece {
  ftxui::Element element;
  bool gap=false;
};
struct PieceState {
  // renderStreaming's stable prefix; for growing text, the settled text.
  std::string stablePrefix;
  int columns=-1;
  bool dim=false;
  // Text the pieces cannot follow exactly (a link reference definition,
  // CR LF line ends, analysis tags): its prefix is rendered whole, as
  // renderStreaming does, until the text is replaced.
  bool whole=false;
  std::size_t prefixVersion=0,prefixPieceVersion=0;
  std::optional<MarkdownPiece> prefixPiece;
  std::vector<MarkdownPiece> settled;
  // Bytes of the stripped text behind `settled` and the open run.
  std::size_t settledBytes=0;
  // Elements started so far; every element after the first has a gap.
  std::size_t elements=0;
  // The unsettled end of the open run of plain blocks, its SGR state, and
  // whether the run has drawn its first piece.
  std::string run;
  AnsiStyle style;
  bool runOpen=false,runDrawn=false;
  // Streaming answers: the stable prefix after the settled pieces.
  std::vector<MarkdownPiece> prefixTail;
  void reset();
};

class Renderer {
public:
  Renderer(FormatServices services,WrapAnsi wrap,int terminalColumns);
  ftxui::Element render(const std::string& content,bool dim=false);
  ftxui::Element renderStreaming(StreamingState& state,
      const std::string& content,bool dim=false);
  // renderStreaming as pieces.
  std::vector<MarkdownPiece> renderStreamingPieces(PieceState& state,
      const std::string& content,bool dim=false);
  // render as pieces, for text that grows between frames.
  std::vector<MarkdownPiece> renderGrowingPieces(PieceState& state,
      const std::string& content,bool dim=false);
  void setTerminalColumns(int columns){columns_=columns;}
  TokenStash& stash(){return stash_;}
private:
  // Lexes `region` (the text after the settled pieces), settles the blocks
  // no later text can change and returns the tokens after them.
  Tokens settle(PieceState& state,std::string_view region,bool dim);
  // The pieces for the open run continued by `tail`, as grouped() closes it.
  std::vector<MarkdownPiece> openPieces(const PieceState& state,const Tokens& tail,
    bool dim);
  // The stable prefix as one piece, the way renderStreaming draws it.
  const MarkdownPiece& wholePrefix(PieceState& state,bool dim);
  FormatServices services_;
  WrapAnsi wrap_;
  int columns_;
  TokenStash stash_;
};

} // namespace batbox::interactive::markdown
