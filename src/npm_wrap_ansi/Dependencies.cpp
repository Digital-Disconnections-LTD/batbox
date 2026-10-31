#include "npm_wrap_ansi/Internal.hpp"
#include "glob/Regex16.hpp"
#include "intl/IndigenousInternal.hpp"
#include <unicode/unorm2.h>
#include <stdexcept>

namespace batbox::npm_wrap_ansi {
namespace {
bool hex(char16_t c){return (c>=u'0'&&c<=u'9')||(c>=u'a'&&c<=u'f')||(c>=u'A'&&c<=u'F');}
// JS RegExp's fixed UTF-16 escapes are code-unit patterns in PCRE2-16.
std::u16string pcrePattern(const std::string& source){
  const auto units=value::utf16CodeUnits(source);std::u16string result;
  for(std::size_t i=0;i<units.size();++i){
    if(units[i]==u'\\'&&i+1<units.size()){
      if(units[i+1]==u'u'&&i+5<units.size()&&hex(units[i+2])&&hex(units[i+3])&&hex(units[i+4])&&hex(units[i+5])){
        result+=u"\\x{";result+=units.substr(i+2,4);result+=u'}';i+=5;continue;
      }
      result+=units[i];result+=units[++i];continue;
    }
    result+=units[i];
  }
  return result;
}
std::uint32_t firstCodepoint(std::u16string_view value){
  if(value.empty())return 0;
  const auto a=static_cast<std::uint32_t>(value[0]);
  if(a>=0xd800&&a<=0xdbff&&value.size()>1){
    const auto b=static_cast<std::uint32_t>(value[1]);
    if(b>=0xdc00&&b<=0xdfff)return 0x10000+((a-0xd800)<<10)+(b-0xdc00);
  }
  return a;
}
const glob::Regex16& ansiRegex(){static const glob::Regex16 result(pcrePattern(ansiPattern()));return result;}
const glob::Regex16& emojiRegex(){static const glob::Regex16 result(pcrePattern(emojiPattern()));return result;}
const std::shared_ptr<intl::Segmenter>& segmenter(){
  static const auto result=intl::indigenousSegmenter("grapheme");return result;
}
bool jsSpace(char16_t c){
  return c==u' '||(c>=u'\t'&&c<=u'\r')||c==0xa0||c==0x1680||
    (c>=0x2000&&c<=0x200a)||c==0x2028||c==0x2029||c==0x202f||
    c==0x205f||c==0x3000||c==0xfeff;
}
}
std::u16string normalize(std::u16string_view input){
  UErrorCode status=U_ZERO_ERROR;const auto* nfc=unorm2_getNFCInstance(&status);
  if(U_FAILURE(status))throw std::runtime_error(u_errorName(status));
  status=U_ZERO_ERROR;
  auto size=unorm2_normalize(nfc,reinterpret_cast<const UChar*>(input.data()),
    static_cast<int32_t>(input.size()),nullptr,0,&status);
  if(status!=U_BUFFER_OVERFLOW_ERROR&&U_FAILURE(status))throw std::runtime_error(u_errorName(status));
  std::u16string result(static_cast<std::size_t>(size),0);status=U_ZERO_ERROR;
  size=unorm2_normalize(nfc,reinterpret_cast<const UChar*>(input.data()),
    static_cast<int32_t>(input.size()),reinterpret_cast<UChar*>(result.data()),size,&status);
  if(U_FAILURE(status))throw std::runtime_error(u_errorName(status));
  return result;
}
std::u16string trimStart(std::u16string_view text){
  std::size_t i=0;while(i<text.size()&&jsSpace(text[i]))++i;
  return std::u16string(text.substr(i));
}
bool blank(std::u16string_view text){
  for(const auto c:text)if(!jsSpace(c))return false;
  return true;
}
std::u16string stripAnsi(std::u16string_view text){
  return ansiRegex().replace(text,[](const glob::RegexMatch&){return std::u16string{};},true);
}
double width(std::u16string_view text){
  if(text.empty())return 0;
  const auto stripped=stripAnsi(text);if(stripped.empty())return 0;
  double result=0;
  auto cursor=segmenter()->segment(value::fromUtf16(stripped));
  while(const auto segment=cursor.next()){
    const auto units=value::utf16CodeUnits(segment->text);
    const auto cp=firstCodepoint(units);
    if(cp<=0x1f||(cp>=0x7f&&cp<=0x9f))continue;
    if(cp>=0x300&&cp<=0x36f)continue;
    if(emojiRegex().search(units,0)){result+=2;continue;}
    result+=eastAsianWidth(cp);
  }
  return result;
}
}
