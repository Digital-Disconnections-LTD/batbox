#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace batbox::value {
namespace {
void encode(std::string& out,std::uint32_t cp) {
  if(cp<0x80)out+=static_cast<char>(cp);
  else if(cp<0x800){out+=static_cast<char>(0xc0|(cp>>6));out+=static_cast<char>(0x80|(cp&63));}
  else if(cp<0x10000){out+=static_cast<char>(0xe0|(cp>>12));out+=static_cast<char>(0x80|((cp>>6)&63));out+=static_cast<char>(0x80|(cp&63));}
  else{out+=static_cast<char>(0xf0|(cp>>18));out+=static_cast<char>(0x80|((cp>>12)&63));out+=static_cast<char>(0x80|((cp>>6)&63));out+=static_cast<char>(0x80|(cp&63));}
}
// Same acceptance rules and messages as utf16CodeUnits, one scalar at a time.
std::uint32_t decodeWtf8(std::string_view text,std::size_t& i) {
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
  return cp;
}
// Length of the run at text[i] that a JSON string can carry unescaped and
// unchanged: printable ASCII other than the quote and the backslash.
std::size_t plainRun(std::string_view text,std::size_t i) {
  constexpr std::uint64_t ones=0x0101010101010101ull,highs=0x8080808080808080ull;
  const auto hasZero=[](std::uint64_t word){return ((word-ones)&~word&highs)!=0;};
  const auto start=i;
  for(std::uint64_t word;i+8<=text.size();i+=8) {
    std::memcpy(&word,text.data()+i,8);
    if((word&highs)||((word-ones*0x20)&~word&highs)||hasZero(word^(ones*'"'))||hasZero(word^(ones*'\\')))break;
  }
  while(i<text.size()) {
    const auto c=static_cast<unsigned char>(text[i]);
    if(c<0x20||c>=0x80||c=='"'||c=='\\')break;
    ++i;
  }
  return i-start;
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
void appendQuoted(std::string& out,std::string_view text) {
  static constexpr char hex[]="0123456789abcdef";
  const auto escapeUnit=[&out](std::uint32_t unit) {
    const char escaped[]={'\\','u',hex[(unit>>12)&15],hex[(unit>>8)&15],hex[(unit>>4)&15],hex[unit&15]};
    out.append(escaped,sizeof(escaped));
  };
  out.reserve(out.size()+text.size()+2);
  out+='"';
  const auto* data=text.data();
  const auto size=text.size();
  std::size_t i=0;
  while(i<size) {
    const auto start=i;
    i+=plainRun(text,i);
    if(i>start)out.append(data+start,i-start);
    if(i==size)break;
    const auto byte=static_cast<unsigned char>(data[i]);
    if(byte<0x80) {
      ++i;
      switch(byte) {
        case '"':out+="\\\"";break;
        case '\\':out+="\\\\";break;
        case '\b':out+="\\b";break;
        case '\f':out+="\\f";break;
        case '\n':out+="\\n";break;
        case '\r':out+="\\r";break;
        case '\t':out+="\\t";break;
        default:escapeUnit(byte);break;
      }
      continue;
    }
    const auto sequence=i;
    std::uint32_t cp=decodeWtf8(text,i);
    if(cp>=0xd800&&cp<=0xdbff) {
      if(i<size) {
        auto next=i;
        const auto low=decodeWtf8(text,next);
        if(low>=0xdc00&&low<=0xdfff) {
          encode(out,0x10000+((cp-0xd800)<<10)+(low-0xdc00));
          i=next;
          continue;
        }
      }
      escapeUnit(cp);
    } else if(cp>=0xdc00&&cp<=0xdfff) {
      escapeUnit(cp);
    } else {
      out.append(data+sequence,i-sequence);
    }
  }
  out+='"';
}
std::string quote(std::string_view text) {
  std::string out;
  appendQuoted(out,text);
  return out;
}
} // namespace batbox::value
