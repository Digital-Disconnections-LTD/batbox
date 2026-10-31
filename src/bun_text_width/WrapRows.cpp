#include "bun_text_width/WrapInternal.hpp"
#include "bun_text_width/Width.hpp"
#include "bun_text_width/Grapheme.hpp"

namespace batbox::bun_text_width {
std::size_t WrapContext::width(std::u16string_view text)const{
  return latin1?widthLatin1(text):widthUtf16(text,false,options.ambiguousIsNarrow);
}
std::size_t Row::width(const WrapContext& context)const{
  return data.empty()?0:context.width(data);
}
void Row::trimLeadingSpaces(){
  std::size_t removeCount=0;
  bool inEscape=false;
  for(const auto c:data){
    if(c==0x1b){inEscape=true;continue;}
    if(inEscape){if(c=='m'||c==0x07)inEscape=false;continue;}
    if(c==' '||c=='\t')++removeCount;
    else break;
  }
  if(!removeCount)return;
  std::u16string next;
  next.reserve(data.size()-removeCount);
  inEscape=false;
  std::size_t removed=0;
  for(const auto c:data){
    if(c==0x1b){inEscape=true;next+=c;continue;}
    if(inEscape){if(c=='m'||c==0x07)inEscape=false;next+=c;continue;}
    if((c==' '||c=='\t')&&removed<removeCount){++removed;continue;}
    next+=c;
  }
  data=std::move(next);
}
namespace {
struct Decoded {std::uint32_t cp;std::size_t length;};
Decoded decode(std::u16string_view word,std::size_t offset,bool latin1){
  const auto first=static_cast<std::uint32_t>(word[offset]);
  if(latin1)return {first,1};
  if(first>=0xd800&&first<=0xdbff&&offset+1<word.size()){
    const auto second=static_cast<std::uint32_t>(word[offset+1]);
    if(second>=0xdc00&&second<=0xdfff)
      return {0x10000+((first-0xd800)<<10)+(second-0xdc00),2};
  }
  // ICU U16_NEXT leaves an unpaired surrogate unchanged. The Zig width helper
  // then classifies it as zero width.
  if(first>=0xd800&&first<=0xdfff)return {first,1};
  return {first,1};
}
}
void wrapWord(Rows& rows,std::u16string_view word,const WrapContext& context){
  bool insideEscape=false,insideLinkEscape=false,insideCsiEscape=false;
  std::size_t visible=rows.back().width(context);
  for(std::size_t i=0;i<word.size();){
    if(word[i]==0x1b){
      insideEscape=true;insideCsiEscape=false;
      if(word.size()-i>4&&word[i+1]==']'&&word[i+2]=='8'&&
         word[i+3]==';'&&word[i+4]==';')insideLinkEscape=true;
      if(word.size()-i>1&&word[i+1]=='[')insideCsiEscape=true;
    }
    const Decoded unit=insideEscape?Decoded{word[i],1}:decode(word,i,context.latin1);
    const auto charWidth=insideEscape?0:codepointWidth(unit.cp,!context.options.ambiguousIsNarrow);
    if(!insideEscape&&visible+charWidth<=context.columns){
      rows.back().data.append(word.substr(i,unit.length));visible+=charWidth;
    }else if(!insideEscape){
      rows.emplace_back();rows.back().data.append(word.substr(i,unit.length));
      visible=charWidth;
    }else rows.back().data+=word[i];
    if(insideEscape){
      if(insideLinkEscape){
        if(word[i]==0x07){insideEscape=insideLinkEscape=false;}
      }else if(insideCsiEscape){
        const auto c=word[i];
        if(c>=0x40&&c<=0x7e&&c!='[')insideEscape=insideCsiEscape=false;
      }else if(word[i]=='m')insideEscape=false;
      ++i;continue;
    }
    if(visible==context.columns&&i+unit.length<word.size()){
      rows.emplace_back();visible=0;
    }
    i+=unit.length;
  }
  if(visible==0&&!rows.back().data.empty()&&rows.size()>1){
    Row last=std::move(rows.back());rows.pop_back();rows.back().data+=last.data;
  }
}
} // namespace batbox::bun_text_width
