#include "terminal_render/IndigenousIndent.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace batbox::terminal_render {
namespace {
bool lineBreak(char16_t c){
  return c==u'\n'||c==u'\r'||c==0x2028||c==0x2029;
}
bool whitespace(char16_t c){
  return (c>=0x09&&c<=0x0d)||c==0x20||c==0xa0||c==0x1680||
    (c>=0x2000&&c<=0x200a)||c==0x2028||c==0x2029||
    c==0x202f||c==0x205f||c==0x3000||c==0xfeff;
}
bool nonblankLine(const std::u16string& text,std::size_t start){
  for(auto i=start;i<text.size()&&!lineBreak(text[i]);i++)
    if(!whitespace(text[i]))return true;
  return false;
}
}
std::string indigenousIndentString(const std::string& text,double count){
  if(count<0)
    throw std::range_error("Expected `count` to be at least 0, got `"+
      value::numberToString(count)+"`");
  if(count==0)return text;
  const auto repeats=std::isnan(count)?0:std::trunc(count);
  if(!std::isfinite(repeats)||repeats<0||
      repeats>static_cast<double>(std::numeric_limits<std::size_t>::max()))
    throw std::range_error("Invalid count value");
  const auto units=value::utf16CodeUnits(text);
  const auto prefix=std::u16string(static_cast<std::size_t>(repeats),u' ');
  std::u16string output;
  for(std::size_t i=0;i<=units.size();i++){
    if(i==0||lineBreak(units[i-1]))
      if(nonblankLine(units,i))output+=prefix;
    if(i<units.size())output+=units[i];
  }
  return value::fromUtf16(output);
}
} // namespace batbox::terminal_render
