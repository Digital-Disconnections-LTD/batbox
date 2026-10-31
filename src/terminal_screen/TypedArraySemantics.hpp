#pragma once
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace batbox::terminal_screen::typed_array {
inline double jsMin(double a,double b){
  if(std::isnan(a)||std::isnan(b))
    return std::numeric_limits<double>::quiet_NaN();
  if(a==0&&b==0)return std::signbit(a)||std::signbit(b)?-0.0:0.0;
  return a<b?a:b;
}
inline double jsMax(double a,double b){
  if(std::isnan(a)||std::isnan(b))
    return std::numeric_limits<double>::quiet_NaN();
  if(a==0&&b==0)return std::signbit(a)&&std::signbit(b)?-0.0:0.0;
  return a>b?a:b;
}
inline std::int32_t toInt32(double number){
  if(!std::isfinite(number)||number==0)return 0;
  auto modulo=std::fmod(std::trunc(number),4294967296.0);
  if(modulo<0)modulo+=4294967296.0;
  return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(modulo));
}
inline double toIntegerOrInfinity(double number){
  if(std::isnan(number)||number==0)return 0;
  return std::trunc(number);
}
inline std::size_t toIndex(double number){
  const auto integer=toIntegerOrInfinity(number);
  if(!std::isfinite(integer)||integer<0||integer>9007199254740991.0||
      integer>=static_cast<double>(std::numeric_limits<std::size_t>::max()))
    throw std::range_error("Invalid typed array length: "+
      value::numberToString(number));
  return static_cast<std::size_t>(integer);
}
inline std::size_t relativeIndex(double number,std::size_t length){
  const auto integer=toIntegerOrInfinity(number);
  if(integer<0){
    if(integer<=-static_cast<double>(length))return 0;
    return length-static_cast<std::size_t>(-integer);
  }
  if(integer>=static_cast<double>(length))return length;
  return static_cast<std::size_t>(integer);
}
template<class T> void fill(std::vector<T>& values,const T& value,
    double start,double end){
  const auto first=relativeIndex(start,values.size());
  const auto last=relativeIndex(end,values.size());
  if(first<last)std::fill(values.begin()+first,values.begin()+last,value);
}
template<class T> void copyWithin(std::vector<T>& values,
    double destination,double start,double end){
  const auto target=relativeIndex(destination,values.size());
  const auto first=relativeIndex(start,values.size());
  const auto last=relativeIndex(end,values.size());
  const auto count=std::min(last>first?last-first:std::size_t{0},
    values.size()-target);
  const std::vector<T> temporary(values.begin()+first,
    values.begin()+first+count);
  std::copy(temporary.begin(),temporary.end(),values.begin()+target);
}
template<class T> void setSubarray(std::vector<T>& destination,
    const std::vector<T>& source,double begin,double end,double offset){
  const auto first=relativeIndex(begin,source.size());
  const auto last=relativeIndex(end,source.size());
  const auto count=last>first?last-first:std::size_t{0};
  const auto target=toIntegerOrInfinity(offset);
  if(!std::isfinite(target)||target<0||
      target>static_cast<double>(destination.size())||
      count>destination.size()-static_cast<std::size_t>(target))
    throw std::range_error("offset is out of bounds");
  const std::vector<T> temporary(source.begin()+first,
    source.begin()+first+count);
  std::copy(temporary.begin(),temporary.end(),
    destination.begin()+static_cast<std::size_t>(target));
}
} // namespace batbox::terminal_screen::typed_array
