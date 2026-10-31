#pragma once
#include "ansi_tokenize/Types.hpp"
#include "text_width/Layout.hpp"
#include <optional>
namespace batbox::text_width {
struct SliceServices {
  std::function<ansi_tokenize::Tokens(const std::string&)> tokenize;
  std::function<ansi_tokenize::Codes(const ansi_tokenize::Codes&)> reduceAnsiCodes,undoAnsiCodes;
  std::function<std::string(const ansi_tokenize::Codes&)> ansiCodesToString;
  std::function<double(const std::optional<std::string>&)> stringWidth;
};
bool isEndCode(const ansi_tokenize::AnsiCode& code);
ansi_tokenize::Codes filterStartCodes(const ansi_tokenize::Codes& codes);
std::string sliceAnsi(const std::string& text,double start,std::optional<double> end,
  const SliceServices& services);
void bindAnsiSlice(WrapServices& wrap,SliceServices services);
}
