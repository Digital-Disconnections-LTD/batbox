#include "bun_text_width/Grapheme.hpp"
#include "bun_text_width/Tables.hpp"
#include <algorithm>

namespace batbox::bun_text_width {
namespace {
// GraphemeBreakNoControl values from the pinned grapheme.zig declaration.
enum class Property : std::uint8_t {
  other,prepend,regionalIndicator,spacingMark,l,v,t,lv,lvt,zwj,zwnj,
  extendedPictographic,emojiModifierBase,emojiModifier,indicExtend,
  indicLinker,indicConsonant
};
Property property(std::uint32_t cp){return static_cast<Property>(graphemeProperty(cp));}
bool isIndicExtend(Property p){return p==Property::indicExtend||p==Property::zwj;}
bool isExtend(Property p){return p==Property::zwnj||p==Property::indicExtend||p==Property::indicLinker;}
bool isPictographic(Property p){return p==Property::extendedPictographic||p==Property::emojiModifierBase;}
bool pictographicSequence(Property p){return p==Property::indicExtend||p==Property::indicLinker||
  p==Property::zwnj||p==Property::zwj||p==Property::extendedPictographic||
  p==Property::emojiModifierBase||p==Property::emojiModifier;}
bool indicSequence(Property p){return p==Property::indicConsonant||p==Property::indicLinker||
  p==Property::indicExtend||p==Property::zwj;}
bool isEmojiBase(std::uint32_t cp){
  if(cp<0x203c||(cp>=0x2c00&&cp<0x1f000)||
     cp==0xfe0e||cp==0xfe0f||cp==0x200d)return false;
  return hasEmojiProperty(cp);
}
bool regional(std::uint32_t cp){return cp>=0x1f1e6&&cp<=0x1f1ff;}
bool skinTone(std::uint32_t cp){return cp>=0x1f3fb&&cp<=0x1f3ff;}
}

bool graphemeBreak(std::uint32_t previous,std::uint32_t next,BreakState& state){
  const Property a=property(previous),b=property(next);
  switch(state){
    case BreakState::regionalIndicator:
      if(a!=Property::regionalIndicator||b!=Property::regionalIndicator)
        state=BreakState::defaultValue;
      break;
    case BreakState::extendedPictographic:
      if(!pictographicSequence(a)||!pictographicSequence(b))state=BreakState::defaultValue;
      break;
    case BreakState::indicConsonant:case BreakState::indicLinker:
      if(!indicSequence(a)||!indicSequence(b))state=BreakState::defaultValue;
      break;
    case BreakState::defaultValue:break;
  }
  if(a==Property::l&&(b==Property::l||b==Property::v||b==Property::lv||b==Property::lvt))return false;
  if((a==Property::lv||a==Property::v)&&(b==Property::v||b==Property::t))return false;
  if((a==Property::lvt||a==Property::t)&&b==Property::t)return false;
  if(b==Property::spacingMark||a==Property::prepend)return false;
  if(a==Property::indicConsonant){
    if(isIndicExtend(b)){state=BreakState::indicConsonant;return false;}
    if(b==Property::indicLinker){state=BreakState::indicLinker;return false;}
  }else if(state==BreakState::indicConsonant){
    if(b==Property::indicLinker){state=BreakState::indicLinker;return false;}
    if(isIndicExtend(b))return false;
    state=BreakState::defaultValue;
  }else if(state==BreakState::indicLinker){
    if(b==Property::indicLinker||isIndicExtend(b))return false;
    if(b==Property::indicConsonant){state=BreakState::defaultValue;return false;}
    state=BreakState::defaultValue;
  }
  if(isPictographic(a)){
    if(isExtend(b)||b==Property::zwj){state=BreakState::extendedPictographic;return false;}
    if(a==Property::emojiModifierBase&&b==Property::emojiModifier){
      state=BreakState::extendedPictographic;return false;}
  }else if(state==BreakState::extendedPictographic){
    if((isExtend(a)||a==Property::emojiModifier)&&(isExtend(b)||b==Property::zwj))return false;
    if(a==Property::zwj&&isPictographic(b)){state=BreakState::defaultValue;return false;}
    state=BreakState::defaultValue;
  }
  if(a==Property::regionalIndicator&&b==Property::regionalIndicator){
    if(state==BreakState::defaultValue){state=BreakState::regionalIndicator;return false;}
    state=BreakState::defaultValue;return true;
  }
  if(isExtend(b)||b==Property::zwj)return false;
  return true;
}

void GraphemeState::reset(std::uint32_t cp,bool ambiguousAsWide){
  first_=last_=cp;count_=1;keycap_=regionalIndicator_=skinTone_=zwj_=vs15_=vs16_=false;
  if(cp<0x80){
    baseWidth_=cp>=0x20&&cp<0x7f?1:0;
    nonEmojiWidth_=baseWidth_;emojiBase_=false;return;
  }
  baseWidth_=codepointWidth(cp,ambiguousAsWide);
  nonEmojiWidth_=baseWidth_;emojiBase_=isEmojiBase(cp);
  keycap_=cp==0x20e3;regionalIndicator_=regional(cp);
  skinTone_=skinTone(cp);zwj_=cp==0x200d;
}
void GraphemeState::add(std::uint32_t cp,bool ambiguousAsWide){
  last_=cp;count_=std::min<int>(255,count_+1);
  keycap_|=cp==0x20e3;regionalIndicator_|=regional(cp);
  skinTone_|=skinTone(cp);zwj_|=cp==0x200d;
  vs15_|=cp==0xfe0e;vs16_|=cp==0xfe0f;
  if(!isZeroWidth(cp))nonEmojiWidth_=std::min<std::uint32_t>(1023,
    nonEmojiWidth_+codepointWidth(cp,ambiguousAsWide));
}
std::uint32_t GraphemeState::width()const{
  if(!count_)return 0;
  if(regionalIndicator_&&count_>=2)return 2;
  if(keycap_)return 2;
  if(regionalIndicator_)return 1;
  if(emojiBase_&&(skinTone_||zwj_))return 2;
  if(vs15_||vs16_){
    if(baseWidth_==2)return 2;
    if(vs16_)return first_<0x80?1:2;
    return 1;
  }
  return nonEmojiWidth_;
}
} // namespace batbox::bun_text_width
