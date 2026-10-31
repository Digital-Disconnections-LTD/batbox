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
std::string styledCharsToString(const StyledChars& chars){
  std::string result;
  for(std::size_t i=0;i<chars.size();++i){
    const auto& item=chars[i];
    if(i==0)result+=ansiCodesToString(item.styles);
    else result+=ansiCodesToString(diffAnsiCodes(chars[i-1].styles,item.styles));
    result+=item.value;
    if(i+1==chars.size())result+=ansiCodesToString(diffAnsiCodes(item.styles,{}));
  }
  return result;
}
} // namespace batbox::ansi_tokenize
