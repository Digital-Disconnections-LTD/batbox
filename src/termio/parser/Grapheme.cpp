#include "termio/parser/Parser.hpp"
#include "intl/IndigenousInternal.hpp"

namespace batbox::termio::parser {
namespace {
bool isEmoji(std::uint32_t cp){
  return (cp>=0x2600&&cp<=0x26ff)||(cp>=0x2700&&cp<=0x27bf)||
    (cp>=0x1f300&&cp<=0x1f9ff)||(cp>=0x1fa00&&cp<=0x1faff)||
    (cp>=0x1f1e0&&cp<=0x1f1ff);
}
bool isEastAsianWide(std::uint32_t cp){
  return (cp>=0x1100&&cp<=0x115f)||(cp>=0x2e80&&cp<=0x9fff)||
    (cp>=0xac00&&cp<=0xd7a3)||(cp>=0xf900&&cp<=0xfaff)||
    (cp>=0xfe10&&cp<=0xfe1f)||(cp>=0xfe30&&cp<=0xfe6f)||
    (cp>=0xff00&&cp<=0xff60)||(cp>=0xffe0&&cp<=0xffe6)||
    (cp>=0x20000&&cp<=0x2fffd)||(cp>=0x30000&&cp<=0x3fffd);
}
std::uint32_t codePointAt(std::u16string_view units,std::size_t index){
  auto cp=static_cast<std::uint32_t>(units[index]);
  if(cp>=0xd800&&cp<=0xdbff&&index+1<units.size()){
    const auto second=static_cast<std::uint32_t>(units[index+1]);
    if(second>=0xdc00&&second<=0xdfff)
      cp=0x10000+((cp-0xd800)<<10)+(second-0xdc00);
  }
  return cp;
}
const std::shared_ptr<intl::Segmenter>& segmenter(){
  static const auto result=intl::indigenousSegmenter("grapheme");return result;
}
}
int graphemeWidth(const std::string& grapheme){
  const auto units=value::utf16CodeUnits(grapheme);
  std::size_t count=0;
  for(std::size_t i=0;i<units.size();++i){
    const auto cp=codePointAt(units,i);if(cp>0xffff)++i;
    if(++count>1)return 2;
  }
  if(units.empty())return 1;
  const auto first=codePointAt(units,0);
  return isEmoji(first)||isEastAsianWide(first)?2:1;
}
std::vector<Grapheme> segmentGraphemes(const std::string& text){
  std::vector<Grapheme> result;
  auto cursor=segmenter()->segment(text);
  while(const auto segment=cursor.next()){
    result.push_back({segment->text,graphemeWidth(segment->text)});
  }
  return result;
}
}
