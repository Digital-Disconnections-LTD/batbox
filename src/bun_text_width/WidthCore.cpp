#include "bun_text_width/Width.hpp"
#include "bun_text_width/Grapheme.hpp"
#include <optional>

namespace batbox::bun_text_width {
namespace {
std::uint32_t latin1Scalar(char16_t c){
  return c<32||(c>=127&&c<=159)||c==0xad?0:1;
}
struct Codepoint {std::uint32_t value=0;std::size_t length=1;bool valid=true;};
Codepoint decode(std::u16string_view text,std::size_t index){
  const auto first=static_cast<std::uint32_t>(text[index]);
  if(first>=0xd800&&first<=0xdbff){
    if(index+1<text.size()){
      const auto second=static_cast<std::uint32_t>(text[index+1]);
      if(second>=0xdc00&&second<=0xdfff)
        return {0x10000+((first-0xd800)<<10)+(second-0xdc00),2,true};
    }
    return {first,1,false};
  }
  if(first>=0xdc00&&first<=0xdfff)return {first,1,false};
  return {first,1,true};
}
} // namespace

std::size_t widthLatin1(std::u16string_view text,bool countAnsiEscapeCodes){
  std::size_t width=0;
  if(countAnsiEscapeCodes){
    for(const auto unit:text)width+=latin1Scalar(unit);
    return width;
  }
  for(std::size_t i=0;i<text.size();){
    if(text[i]!=0x1b){width+=latin1Scalar(text[i]);++i;continue;}
    if(i+1==text.size())return width;
    const auto next=text[i+1];
    if(next=='['){
      i+=2;
      while(i<text.size()){
        const auto c=text[i++];
        if(c>=0x40&&c<=0x7e)break;
      }
    }else if(next==']'){
      i+=2;
      while(i<text.size()){
        if(text[i]==0x07){++i;break;}
        if(text[i]==0x1b&&i+1<text.size()&&text[i+1]=='\\'){i+=2;break;}
        ++i;
      }
    }else ++i;
  }
  return width;
}

std::size_t widthUtf16(std::u16string_view text,bool countAnsiEscapeCodes,
  bool ambiguousIsNarrow){
  const bool ambiguousAsWide=!ambiguousIsNarrow;
  std::size_t width=0;
  std::optional<std::uint32_t> previousVisible,previousLiteral;
  BreakState breakState=BreakState::defaultValue;
  GraphemeState grapheme;
  bool sawEsc=false,sawCsi=false,sawOsc=false;
  for(std::size_t i=0;i<text.size();){
    // The Zig UTF-16 routine counts an ASCII run in bulk, retaining its last
    // codepoint so a following combining mark can join that grapheme.
    if(!sawEsc&&!sawCsi&&!sawOsc&&text[i]<0x80){
      std::size_t end=i;
      while(end<text.size()&&text[end]<0x80&&
            (countAnsiEscapeCodes||text[end]!=0x1b))++end;
      if(end>i){
        if(grapheme.active())width+=grapheme.width();
        for(std::size_t j=i;j+1<end;++j)
          if(text[j]>=0x20&&text[j]<0x7f)++width;
        const auto last=static_cast<std::uint32_t>(text[end-1]);
        grapheme.reset(last,ambiguousAsWide);
        previousVisible=last;previousLiteral=last;
        breakState=BreakState::defaultValue;
        i=end;
        if(!countAnsiEscapeCodes&&i<text.size()&&text[i]==0x1b){
          sawEsc=true;previousLiteral=0x1b;++i;
        }
        continue;
      }
    }
    const Codepoint decoded=decode(text,i);
    i+=decoded.length;
    if(!decoded.valid)continue;
    const auto cp=decoded.value;
    if(cp<0x80){
      if(sawOsc){
        if(cp==0x07||(cp=='\\'&&previousLiteral==0x1b)){
          sawEsc=sawOsc=false;
        }
        previousLiteral=cp;continue;
      }
      if(sawCsi){
        if(cp>=0x40&&cp<=0x7e)sawEsc=sawCsi=false;
        previousLiteral=cp;continue;
      }
      if(sawEsc){
        if(cp=='['){sawCsi=true;previousLiteral=cp;continue;}
        if(cp==']'){sawOsc=true;previousLiteral=cp;continue;}
        if(cp==0x1b){previousLiteral=cp;continue;}
        width+=codepointWidth(cp,ambiguousAsWide);
        sawEsc=false;previousLiteral=cp;continue;
      }
      if(!countAnsiEscapeCodes&&cp==0x1b){
        sawEsc=true;previousLiteral=cp;continue;
      }
    }else{
      if(sawOsc){previousLiteral=cp;continue;}
      if(sawCsi){sawEsc=sawCsi=false;previousLiteral=cp;continue;}
      if(sawEsc)sawEsc=false;
    }
    if(previousVisible){
      if(graphemeBreak(*previousVisible,cp,breakState)){
        width+=grapheme.width();
        grapheme.reset(cp,ambiguousAsWide);
      }else grapheme.add(cp,ambiguousAsWide);
    }else grapheme.reset(cp,ambiguousAsWide);
    previousVisible=cp;previousLiteral=cp;
  }
  width+=grapheme.width();
  return width;
}
} // namespace batbox::bun_text_width
