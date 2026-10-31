#include "ansi_tokenize/AnsiTokenize.hpp"
#include "ansi_tokenize/Fullwidth.hpp"
#include "ansi_tokenize/TokenizeInternal.hpp"
#include "intl/IndigenousInternal.hpp"
#include <cmath>

namespace batbox::ansi_tokenize {
namespace {
std::uint32_t firstCodePoint(const std::u16string& units){
  if(units.empty())return 0;
  const auto first=static_cast<std::uint32_t>(units[0]);
  if(first>=0xd800&&first<=0xdbff&&units.size()>1){
    const auto second=static_cast<std::uint32_t>(units[1]);
    if(second>=0xdc00&&second<=0xdfff)
      return 0x10000+((first-0xd800)<<10)+(second-0xdc00);
  }
  return first;
}
std::shared_ptr<intl::Segmenter> segmenter(){
  static const auto instance=intl::indigenousSegmenter("grapheme");
  return instance;
}
}
Tokens tokenizeUntil(const std::string& text,double endChar){
  Tokens result;
  const auto units=value::utf16CodeUnits(text);
  std::size_t codeEndIndex=0;
  double visible=0;
  auto cursor=segmenter()->segment(text);
  while(const auto segment=cursor.next()){
    const auto index=segment->index;
    if(index<codeEndIndex)continue;
    const auto part=value::utf16CodeUnits(segment->text);
    const auto cp=firstCodePoint(part);
    if(cp==0x1b||cp==0x9b){
      std::optional<std::u16string> code;
      const auto next=index+1<units.size()?static_cast<std::uint32_t>(units[index+1]):0xffffffff;
      if(next==']'){
        code=parseLinkCode(units,index);
        if(code){
          auto raw=value::fromUtf16(*code);
          auto ref=std::make_shared<AnsiCode>(AnsiCode{"ansi",raw,getEndCode(raw)});
          result.push_back(Token{"ansi",std::nullopt,false,ref,{}});
        }else{
          code=parseOscSequence(units,index);
          if(code)result.push_back(Token{"control",std::nullopt,false,nullptr,
            value::fromUtf16(*code)});
        }
      }else if(next=='['){
        code=parseSgrSequence(units,index);
        if(code)for(const auto& component:splitCompoundSgr(*code)){
          const auto raw=value::fromUtf16(component);
          auto ref=std::make_shared<AnsiCode>(AnsiCode{"ansi",raw,getEndCode(raw)});
          result.push_back(Token{"ansi",std::nullopt,false,ref,{}});
        }
      }
      if(code){codeEndIndex=index+code->size();continue;}
    }
    const bool fullWidth=isFullwidthCodePoint(cp)||part.find(u'\ufe0f')!=std::u16string::npos||
      (cp>=0x1f1e6&&cp<=0x1f1ff);
    result.push_back(Token{"char",segment->text,fullWidth,nullptr,{}});
    visible+=fullWidth?2:1;
    if(visible>=endChar)break;
  }
  return result;
}
Tokens tokenize(const std::string& text){
  return tokenizeUntil(text,std::numeric_limits<double>::infinity());
}
} // namespace batbox::ansi_tokenize
