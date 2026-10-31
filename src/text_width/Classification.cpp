#include "text_width/StringWidth.hpp"
#include "text_width/CodePoints.hpp"
#include <limits>
namespace batbox::text_width {
bool needsSegmentation(const std::string& str){
  for(const auto cp:codePoints(str)){
    if(cp>=0x1f300&&cp<=0x1faff)return true;
    if(cp>=0x2600&&cp<=0x27bf)return true;
    if(cp>=0x1f1e6&&cp<=0x1f1ff)return true;
    if(cp>=0xfe00&&cp<=0xfe0f)return true;
    if(cp==0x200d)return true;
  }
  return false;
}
double getEmojiWidth(const std::string& grapheme){
  const auto points=codePoints(grapheme);
  const auto first=points.empty()?std::numeric_limits<std::uint32_t>::max():points.front();
  if(first>=0x1f1e6&&first<=0x1f1ff){
    std::size_t count=0;for(const auto unused:points){(void)unused;++count;}
    return count==1?1:2;
  }
  const auto units=value::utf16CodeUnits(grapheme);
  if(units.size()==2){const auto second=units[1];
    if(second==0xfe0f&&((first>=0x30&&first<=0x39)||first==0x23||first==0x2a))return 1;
  }
  return 2;
}
bool isZeroWidth(std::uint32_t cp){
  if(cp>=0x20&&cp<0x7f)return false;
  if(cp>=0xa0&&cp<0x0300)return cp==0x00ad;
  if(cp<=0x1f||(cp>=0x7f&&cp<=0x9f))return true;
  if((cp>=0x200b&&cp<=0x200d)||cp==0xfeff||(cp>=0x2060&&cp<=0x2064))return true;
  if((cp>=0xfe00&&cp<=0xfe0f)||(cp>=0xe0100&&cp<=0xe01ef))return true;
  if((cp>=0x0300&&cp<=0x036f)||(cp>=0x1ab0&&cp<=0x1aff)||
     (cp>=0x1dc0&&cp<=0x1dff)||(cp>=0x20d0&&cp<=0x20ff)||
     (cp>=0xfe20&&cp<=0xfe2f))return true;
  if(cp>=0x0900&&cp<=0x0d4f){const auto offset=cp&0x7f;
    if(offset<=0x03)return true;
    if(offset>=0x3a&&offset<=0x4f)return true;
    if(offset>=0x51&&offset<=0x57)return true;
    if(offset>=0x62&&offset<=0x63)return true;
  }
  if(cp==0x0e31||(cp>=0x0e34&&cp<=0x0e3a)||(cp>=0x0e47&&cp<=0x0e4e)||
     cp==0x0eb1||(cp>=0x0eb4&&cp<=0x0ebc)||(cp>=0x0ec8&&cp<=0x0ecd))return true;
  if((cp>=0x0600&&cp<=0x0605)||cp==0x06dd||cp==0x070f||cp==0x08e2)return true;
  if(cp>=0xd800&&cp<=0xdfff)return true;
  if(cp>=0xe0000&&cp<=0xe007f)return true;
  return false;
}
} // namespace batbox::text_width
