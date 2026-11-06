#include "ansi_tokenize/AnsiTokenize.hpp"

namespace batbox::ansi_tokenize {
StyledChars styledCharsFromTokens(const Tokens& tokens){
  Codes codes;
  StyledChars result;
  for(const auto& token:tokens){
    if(token.type=="ansi")codes=reduceAnsiCodesIncremental(codes,{token.ansi});
    else if(token.type=="char")
      result.push_back(StyledChar{"char",token.value.value_or(""),token.fullWidth,codes});
  }
  return result;
}
} // namespace batbox::ansi_tokenize
