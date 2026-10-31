#include "media/Media.hpp"
#include "runtime/JsonValue.hpp"
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace batbox::media {
namespace {
std::string fixedTwo(double value){
  if(!std::isfinite(value)||std::abs(value)>=1e21)return value::numberToString(value);
  const bool negative=std::signbit(value);value=std::abs(value);
  // Binary64 has 53 significant bits. x87's wider mantissa can multiply by
  // 100 exactly for the image dimension ratios used here; JS toFixed rounds
  // an exact halfway result away from zero.
  const long double scaled=static_cast<long double>(value)*100.0L;
  if(scaled>=static_cast<long double>(std::numeric_limits<std::uint64_t>::max())){
    char digits[128];
    const auto [end,error]=std::to_chars(digits,digits+sizeof(digits),value,
      std::chars_format::fixed,2);
    if(error!=std::errc{})throw std::runtime_error("Image scale formatting failed");
    return (negative?"-":"")+std::string(digits,end);
  }
  const long double lower=std::floor(scaled);
  const auto rounded=static_cast<std::uint64_t>(lower+(scaled-lower>=0.5L?1:0));
  const auto remainder=rounded%100;
  return (negative?"-":"")+std::to_string(rounded/100)+"."+
    (remainder<10?"0":"")+std::to_string(remainder);
}
}
std::string detectImageFormatFromBuffer(const std::string& buffer){
  if(buffer.size()<4)return "image/png";
  const auto* bytes=reinterpret_cast<const unsigned char*>(buffer.data());
  if(bytes[0]==0x89&&bytes[1]==0x50&&bytes[2]==0x4e&&bytes[3]==0x47)return "image/png";
  if(bytes[0]==0xff&&bytes[1]==0xd8&&bytes[2]==0xff)return "image/jpeg";
  if(bytes[0]==0x47&&bytes[1]==0x49&&bytes[2]==0x46)return "image/gif";
  if(bytes[0]==0x52&&bytes[1]==0x49&&bytes[2]==0x46&&bytes[3]==0x46&&
     buffer.size()>=12&&bytes[8]==0x57&&bytes[9]==0x45&&bytes[10]==0x42&&bytes[11]==0x50)
    return "image/webp";
  return "image/png";
}

std::optional<std::string> createImageMetadataText(const Json& dims,
  const std::optional<std::string>& sourcePath){
  const auto value=[&](const char* key)->double{return value::numberOr(value::property(dims,key));};
  const double originalWidth=value("originalWidth"),originalHeight=value("originalHeight"),
    displayWidth=value("displayWidth"),displayHeight=value("displayHeight");
  if(!originalWidth||!originalHeight||!displayWidth||!displayHeight||
     displayWidth<=0||displayHeight<=0){
    if(sourcePath&&!sourcePath->empty())return "[Image source: "+*sourcePath+"]";
    return std::nullopt;
  }
  const bool resized=originalWidth!=displayWidth||originalHeight!=displayHeight;
  if(!resized&&(!sourcePath||sourcePath->empty()))return std::nullopt;
  std::string result="[Image: ";
  if(sourcePath&&!sourcePath->empty())result+="source: "+*sourcePath;
  if(resized){
    if(sourcePath&&!sourcePath->empty())result+=", ";
    result+="original "+value::numberToString(originalWidth)+"x"+value::numberToString(originalHeight)+
      ", displayed at "+value::numberToString(displayWidth)+"x"+value::numberToString(displayHeight)+
      ". Multiply coordinates by "+fixedTwo(originalWidth/displayWidth)+" to map to original image.";
  }
  return result+"]";
}
} // namespace batbox::media
