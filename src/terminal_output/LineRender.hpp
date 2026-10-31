#pragma once
#include "terminal_output/Output.hpp"
#include "ansi_tokenize/AnsiTokenize.hpp"
#include "intl/Intl.hpp"
#include <functional>

namespace batbox::text_width {class StringWidth;}
namespace batbox::terminal_output {
class BidiReorder;
struct LineServices {
  std::function<ansi_tokenize::Tokens(const std::string&)> tokenize;
  std::function<ansi_tokenize::StyledChars(const ansi_tokenize::Tokens&)>
    styledCharsFromTokens;
  std::function<std::shared_ptr<intl::Segmenter>()> getGraphemeSegmenter;
  std::function<double(const std::string&)> stringWidth;
  std::function<ClusteredCharsRef(const ClusteredCharsRef&)> reorderBidi;
};
bool stylesEqual(const ansi_tokenize::Codes& a,
  const ansi_tokenize::Codes& b);
void flushBuffer(const std::string& buffer,const ansi_tokenize::Codes& styles,
  StylePool& stylePool,ClusteredChars& out,const LineServices& services);
ClusteredCharsRef styledCharsWithGraphemeClustering(
  const ansi_tokenize::StyledChars& chars,StylePool& stylePool,
  const LineServices& services);
double writeLineToScreen(terminal_screen::Screen& screen,
  const std::string& line,double x,double y,double screenWidth,
  StylePool& stylePool,CharStash& charStash,const LineServices& services);
LineServices makeIndigenousLineServices(std::shared_ptr<intl::Stash> intl,
  std::shared_ptr<text_width::StringWidth> width,
  std::shared_ptr<BidiReorder> bidi);
} // namespace batbox::terminal_output
