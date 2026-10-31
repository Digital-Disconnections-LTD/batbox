#include "npm_wrap_ansi/Internal.hpp"
#include "glob/Regex16.hpp"
#include <cstdlib>
#include <optional>

namespace batbox::npm_wrap_ansi {
namespace {
std::optional<int> closeCode(double code){
  if(code==0)return 0;
  if(code==1||code==2)return 22;
  if(code==3)return 23;
  if(code==4)return 24;
  if(code==53)return 55;
  if(code==7)return 27;
  if(code==8)return 28;
  if(code==9)return 29;
  if((code>=30&&code<=37)||(code>=90&&code<=97))return 39;
  if((code>=40&&code<=47)||(code>=100&&code<=107))return 49;
  return std::nullopt;
}
std::u16string sgr(int code){return u"\x1b["+value::utf16CodeUnits(std::to_string(code))+u'm';}
std::u16string link(std::u16string_view uri){return u"\x1b]8;;"+std::u16string(uri)+u'\x07';}
const glob::Regex16& escapePattern(){
  // Search after each escape; the regex is intentionally unanchored.
  static const glob::Regex16 pattern(u"(?:\\[(\\d+)m|\\]8;;(.*)\x07)");return pattern;
}
std::vector<std::u16string> codepoints(std::u16string_view text){
  std::vector<std::u16string> result;
  for(std::size_t i=0;i<text.size();++i){
    if(text[i]>=0xd800&&text[i]<=0xdbff&&i+1<text.size()&&
       text[i+1]>=0xdc00&&text[i+1]<=0xdfff)result.emplace_back(text.substr(i++,2));
    else result.emplace_back(text.substr(i,1));
  }
  return result;
}
double asciiNumber(const std::u16string& digits){
  const auto bytes=value::fromUtf16(digits);return std::strtod(bytes.c_str(),nullptr);
}
}
std::u16string preserveAnsi(std::u16string_view rows){
  std::u16string result;
  std::optional<double> escapeCode;
  std::optional<std::u16string> escapeUrl;
  const auto pre=codepoints(rows);
  std::size_t preStringIndex=0;
  for(std::size_t index=0;index<pre.size();++index){
    const auto& character=pre[index];result+=character;
    if(character==u"\x1b"||character==u"\x9b"){
      glob::RegexMatch match;
      if(escapePattern().search(rows,preStringIndex,&match)){
        if(match.groups.size()>1&&match.groups[1]){
          const auto code=asciiNumber(*match.groups[1]);
          escapeCode=code==39?std::nullopt:std::optional<double>(code);
        }else if(match.groups.size()>2&&match.groups[2]){
          escapeUrl=match.groups[2]->empty()?std::nullopt:
            std::optional<std::u16string>(*match.groups[2]);
        }
      }
    }
    const auto code=escapeCode?closeCode(*escapeCode):std::nullopt;
    if(index+1<pre.size()&&pre[index+1]==u"\n"){
      if(escapeUrl)result+=link(u"");
      if(escapeCode&&*escapeCode!=0&&code)result+=sgr(*code);
    }else if(character==u"\n"){
      if(escapeCode&&*escapeCode!=0&&code)result+=sgr(static_cast<int>(*escapeCode));
      if(escapeUrl)result+=link(*escapeUrl);
    }
    preStringIndex+=character.size();
  }
  return result;
}
}
