#include "bun_text_width/Grapheme.hpp"
#include "bun_text_width/Tables.hpp"

namespace batbox::bun_text_width {
bool isZeroWidth(std::uint32_t cp){
  if(cp<=0x1f||(cp>=0x7f&&cp<=0x9f)||cp==0xad)return true;
  if(cp>=0x300&&cp<=0x36f)return true;
  if(cp>=0x200b&&cp<=0x200f)return true;
  if(cp>=0x2060&&cp<=0x2064)return true;
  if(cp>=0x20d0&&cp<=0x20ff)return true;
  if(cp>=0xfe00&&cp<=0xfe0f)return true;
  if(cp>=0xfe20&&cp<=0xfe2f)return true;
  if(cp==0xfeff)return true;
  if(cp>=0xd800&&cp<=0xdfff)return true;
  if((cp>=0x600&&cp<=0x605)||cp==0x6dd||cp==0x70f||cp==0x8e2)return true;
  if(cp>=0x900&&cp<=0xd4f){
    const auto offset=cp&0x7f;
    if(offset<=0x02)return true;
    if(offset>=0x3a&&offset<=0x4d&&offset!=0x3d)return true;
    if(offset>=0x51&&offset<=0x57)return true;
    if(offset>=0x62&&offset<=0x63)return true;
  }
  if(cp==0xe31||(cp>=0xe34&&cp<=0xe3a)||(cp>=0xe47&&cp<=0xe4e))return true;
  if(cp==0xeb1||(cp>=0xeb4&&cp<=0xebc)||(cp>=0xec8&&cp<=0xecd))return true;
  if(cp>=0x1ab0&&cp<=0x1aff)return true;
  if(cp>=0x1dc0&&cp<=0x1dff)return true;
  if(cp>=0xe0000&&cp<=0xe007f)return true;
  if(cp>=0xe0100&&cp<=0xe01ef)return true;
  return false;
}
std::uint8_t codepointWidth(std::uint32_t cp,bool ambiguousAsWide){
  if(isZeroWidth(cp))return 0;
  if(isFullWidth(cp))return 2;
  if(ambiguousAsWide&&isAmbiguous(cp))return 2;
  return 1;
}
} // namespace batbox::bun_text_width
