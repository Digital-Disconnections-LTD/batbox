#pragma once
#include "ansi_tokenize/Types.hpp"
#include "runtime/JsonValue.hpp"
#include <limits>
#include <string>

namespace batbox::ansi_tokenize {
std::string getEndCode(const std::string& code);
std::string getLinkStartCode(const std::string& url,const Json& params=Json());
std::string ansiCodesToString(const Codes& codes);
bool isIntensityCode(const AnsiCode& code);
Codes reduceAnsiCodes(const Codes& codes);
Codes reduceAnsiCodesIncremental(const Codes& codes,const Codes& newCodes);
Codes undoAnsiCodes(const Codes& codes);
Codes diffAnsiCodes(const Codes& from,const Codes& to);
Tokens tokenize(const std::string& text);
Tokens tokenizeUntil(const std::string& text,double endChar);
struct StyledChar {
  std::string type="char",value;
  bool fullWidth=false;
  Codes styles;
};
using StyledChars=std::vector<StyledChar>;
StyledChars styledCharsFromTokens(const Tokens& tokens);
std::string styledCharsToString(const StyledChars& chars);
} // namespace batbox::ansi_tokenize
