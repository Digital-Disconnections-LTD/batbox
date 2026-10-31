#include "terminal_screen/HyperlinkStyles.hpp"
#include "termio/parser/AnsiLeaf.hpp"
#include <string_view>

namespace batbox::terminal_screen {
namespace {
const std::string prefix=std::string(termio::parser::ansi::ESC)+"]8"+
  std::string(termio::parser::ansi::SEP);
const std::string fullPrefix=prefix+std::string(termio::parser::ansi::SEP);
bool matchOsc8(const std::string& code,std::string* uri=nullptr){
  if(code.size()<fullPrefix.size()+1||
      code.compare(0,fullPrefix.size(),fullPrefix)!=0||
      code.back()!=termio::parser::ansi::BEL.front())return false;
  const auto begin=fullPrefix.size();
  if(code.find(termio::parser::ansi::BEL.front(),begin)!=code.size()-1)
    return false;
  if(uri)*uri=code.substr(begin,code.size()-begin-1);
  return true;
}
}
std::optional<std::string> extractHyperlinkFromStyles(
    const ansi_tokenize::Codes& styles){
  for(const auto& style:styles){
    const auto& code=style->code;
    if(code.size()<5||code.compare(0,prefix.size(),prefix)!=0)continue;
    std::string uri;
    if(matchOsc8(code,&uri))return uri.empty()?std::optional<std::string>{}:
      std::optional<std::string>{std::move(uri)};
  }
  return {};
}
ansi_tokenize::Codes filterOutHyperlinkStyles(
    const ansi_tokenize::Codes& styles){
  ansi_tokenize::Codes result;
  result.reserve(styles.size());
  for(const auto& style:styles){
    const auto& code=style->code;
    if(code.compare(0,prefix.size(),prefix)!=0||!matchOsc8(code))
      result.push_back(style);
  }
  return result;
}
} // namespace batbox::terminal_screen
