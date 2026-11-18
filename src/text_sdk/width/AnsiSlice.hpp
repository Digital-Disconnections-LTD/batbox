#pragma once
#include "text_sdk/ansi_tokenize/Types.hpp"
#include <functional>
#include <optional>
#include <string>
namespace batbox::text_sdk::width {
struct SliceServices {
  std::function<text_sdk::ansi_tokenize::Tokens(const std::string&)> tokenize;
  std::function<text_sdk::ansi_tokenize::Codes(const text_sdk::ansi_tokenize::Codes&)> reduceAnsiCodes,undoAnsiCodes;
  std::function<std::string(const text_sdk::ansi_tokenize::Codes&)> ansiCodesToString;
  std::function<double(const std::optional<std::string>&)> stringWidth;
};
bool isEndCode(const text_sdk::ansi_tokenize::AnsiCode& code);
text_sdk::ansi_tokenize::Codes filterStartCodes(const text_sdk::ansi_tokenize::Codes& codes);
std::string sliceAnsi(const std::string& text,double start,std::optional<double> end,
  const SliceServices& services);
}
