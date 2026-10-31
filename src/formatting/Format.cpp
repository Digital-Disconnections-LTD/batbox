#include "formatting/Format.hpp"
#include "runtime/JsonValue.hpp"
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace batbox::formatting {
namespace {
std::string fixedOne(double x){
  if(std::isnan(x)||std::isinf(x))return value::numberToString(x);
  if(std::abs(x)>=1e21)return value::numberToString(x);
  if(x==0)return "0.0";
  const bool negative=std::signbit(x);x=std::abs(x);
  if(x<0.05)return (negative?"-":"")+std::string("0.0");
  if(x>=9007199254740992.0){
    char data[256];auto [end,error]=std::to_chars(data,data+sizeof(data),x,std::chars_format::fixed,1);
    if(error!=std::errc{})throw std::runtime_error("fixed conversion failed");
    return (negative?"-":"")+std::string(data,end);
  }
  // x is a binary64 value. Extended precision holds its 53-bit significand
  // multiplied by ten exactly, including half-up decimal ties.
  const long double scaled=static_cast<long double>(x)*10.0L;
  const long double floorValue=std::floor(scaled);
  const auto rounded=static_cast<std::uint64_t>(floorValue+(scaled-floorValue>=0.5L?1:0));
  return (negative?"-":"")+std::to_string(rounded/10)+"."+std::to_string(rounded%10);
}
std::string trimZero(std::string value){
  if(value.size()>=2&&value.substr(value.size()-2)==".0")value.resize(value.size()-2);
  return value;
}
}
std::string formatFileSize(double sizeInBytes){
  const double kb=sizeInBytes/1024;
  if(kb<1)return value::numberToString(sizeInBytes)+" bytes";
  if(kb<1024)return trimZero(fixedOne(kb))+"KB";
  const double mb=kb/1024;
  if(mb<1024)return trimZero(fixedOne(mb))+"MB";
  const double gb=mb/1024;
  return trimZero(fixedOne(gb))+"GB";
}
} // namespace batbox::formatting
