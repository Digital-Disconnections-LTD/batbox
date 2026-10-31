#include "terminal_log_update/Internal.hpp"
#include "runtime/JsonValue.hpp"
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace batbox::terminal_log_update {
namespace {
bool jsWhitespace(char16_t c){
  return (c>=0x0009&&c<=0x000D)||c==0x0020||c==0x00A0||
    c==0x1680||(c>=0x2000&&c<=0x200A)||c==0x2028||
    c==0x2029||c==0x202F||c==0x205F||c==0x3000||c==0xFEFF;
}
}
std::string trimEndJs(const std::string& text){
  auto units=value::utf16CodeUnits(text);
  while(!units.empty()&&jsWhitespace(units.back()))units.pop_back();
  return value::fromUtf16(units);
}
std::string readLine(const terminal_screen::Screen& screen,double y){
  std::string line;
  for(double x=0;x<screen.width;x+=1)
    line+=terminal_screen::charInCellAt(screen,x,y).value_or(" ");
  return trimEndJs(line);
}
std::string fixedOne(double value){
  if(!std::isfinite(value)||std::abs(value)>=1e21)
    return value::numberToString(value);
  const bool negative=value<0;value=std::abs(value);
  const long double scaled=static_cast<long double>(value)*10.0L;
  if(scaled>=static_cast<long double>(
      std::numeric_limits<std::uint64_t>::max())){
    char digits[128];
    const auto [end,error]=std::to_chars(digits,digits+sizeof(digits),value,
      std::chars_format::fixed,1);
    if(error!=std::errc{})
      throw std::runtime_error("Render timing format failed");
    return (negative?"-":"")+std::string(digits,end);
  }
  const auto lower=std::floor(scaled);
  const auto rounded=static_cast<std::uint64_t>(
    lower+(scaled-lower>=0.5L?1:0));
  return (negative?"-":"")+std::to_string(rounded/10)+"."+
    std::to_string(rounded%10);
}
} // namespace batbox::terminal_log_update
