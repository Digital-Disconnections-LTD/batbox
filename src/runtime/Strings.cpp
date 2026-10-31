#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace batbox::value {
namespace {
void encode(std::string& out,std::uint32_t cp) {
  if(cp<0x80)out+=static_cast<char>(cp);
  else if(cp<0x800){out+=static_cast<char>(0xc0|(cp>>6));out+=static_cast<char>(0x80|(cp&63));}
  else if(cp<0x10000){out+=static_cast<char>(0xe0|(cp>>12));out+=static_cast<char>(0x80|((cp>>6)&63));out+=static_cast<char>(0x80|(cp&63));}
  else{out+=static_cast<char>(0xf0|(cp>>18));out+=static_cast<char>(0x80|((cp>>12)&63));out+=static_cast<char>(0x80|((cp>>6)&63));out+=static_cast<char>(0x80|(cp&63));}
}
double integer(double value){return std::isnan(value)?0:std::trunc(value);}
std::size_t substringIndex(double value,std::size_t length) {
  value=integer(value);
  if(value<=0)return 0;
  if(value>=static_cast<double>(length))return length;
  return static_cast<std::size_t>(value);
}
std::size_t sliceIndex(double value,std::size_t length) {
  value=integer(value);
  if(value<0)value=std::max(0.0,static_cast<double>(length)+value);
  return substringIndex(value,length);
}
} // namespace
std::u16string utf16CodeUnits(std::string_view text) {
  std::u16string out;
  for(std::size_t i=0;i<text.size();) {
    const auto first=static_cast<unsigned char>(text[i++]);
    std::uint32_t cp=first;
    int remaining=0;
    if((first&0xe0)==0xc0){cp=first&31;remaining=1;}
    else if((first&0xf0)==0xe0){cp=first&15;remaining=2;}
    else if((first&0xf8)==0xf0){cp=first&7;remaining=3;}
    else if(first>=0x80)throw std::invalid_argument("Invalid WTF-8");
    const auto width=remaining;
    while(remaining--) {
      if(i==text.size())throw std::invalid_argument("Incomplete WTF-8");
      const auto next=static_cast<unsigned char>(text[i++]);
      if((next&0xc0)!=0x80)throw std::invalid_argument("Invalid WTF-8 continuation");
      cp=(cp<<6)|(next&63);
    }
    if((width==1&&cp<0x80)||(width==2&&cp<0x800)||(width==3&&cp<0x10000)||cp>0x10ffff)
      throw std::invalid_argument("Invalid WTF-8 scalar");
    if(cp<=0xffff)out+=static_cast<char16_t>(cp);
    else{cp-=0x10000;out+=static_cast<char16_t>(0xd800+(cp>>10));out+=static_cast<char16_t>(0xdc00+(cp&1023));}
  }
  return out;
}
std::string fromUtf16(std::u16string_view text) {
  std::string out;
  for(std::size_t i=0;i<text.size();++i) {
    std::uint32_t cp=text[i];
    if(cp>=0xd800&&cp<=0xdbff&&i+1<text.size()&&text[i+1]>=0xdc00&&text[i+1]<=0xdfff)
      cp=0x10000+((cp-0xd800)<<10)+(text[++i]-0xdc00);
    encode(out,cp);
  }
  return out;
}
std::string substring(std::string_view text,double start,double end) {
  const auto units=utf16CodeUnits(text);
  auto first=substringIndex(start,units.size()),last=substringIndex(end,units.size());
  if(first>last)std::swap(first,last);
  return fromUtf16(std::u16string_view(units).substr(first,last-first));
}
std::string slice(std::string_view text,double start,double end) {
  const auto units=utf16CodeUnits(text);
  const auto first=sliceIndex(start,units.size()),last=sliceIndex(end,units.size());
  return last<=first ? "" : fromUtf16(std::u16string_view(units).substr(first,last-first));
}
std::string quote(std::string_view text) {
  const auto units=utf16CodeUnits(text);
  std::string out="\"";
  constexpr char hex[]="0123456789abcdef";
  for(std::size_t i=0;i<units.size();++i) {
    std::uint32_t cp=units[i];
    if(cp=='"'||cp=='\\'){out+='\\';out+=static_cast<char>(cp);continue;}
    if(cp=='\b'){out+="\\b";continue;}
    if(cp=='\f'){out+="\\f";continue;}
    if(cp=='\n'){out+="\\n";continue;}
    if(cp=='\r'){out+="\\r";continue;}
    if(cp=='\t'){out+="\\t";continue;}
    if(cp>=0xd800&&cp<=0xdbff&&i+1<units.size()&&units[i+1]>=0xdc00&&units[i+1]<=0xdfff)
      cp=0x10000+((cp-0xd800)<<10)+(units[++i]-0xdc00);
    if(cp<0x20||(cp>=0xd800&&cp<=0xdfff)) {
      out+="\\u";
      for(int shift=12;shift>=0;shift-=4)out+=hex[(cp>>shift)&15];
    } else encode(out,cp);
  }
  return out+'"';
}
} // namespace batbox::value
