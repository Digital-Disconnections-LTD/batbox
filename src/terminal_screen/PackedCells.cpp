#include "terminal_screen/Screen.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace batbox::terminal_screen {
std::optional<std::int32_t> PackedCells::word(std::int64_t index)const{
  if(index<0||static_cast<std::uint64_t>(index)>=length32())return {};
  const auto packed=data_[static_cast<std::size_t>(index)>>1];
  const auto bits=static_cast<std::uint32_t>(
    index&1?packed>>32:packed);
  return std::bit_cast<std::int32_t>(bits);
}
void PackedCells::setWord(std::int64_t index,std::int32_t value){
  if(index<0||static_cast<std::uint64_t>(index)>=length32())return;
  auto& packed=data_[static_cast<std::size_t>(index)>>1];
  const auto bits=static_cast<std::uint32_t>(value);
  if(index&1)packed=(packed&0xffffffffull)|
    (static_cast<std::uint64_t>(bits)<<32);
  else packed=(packed&0xffffffff00000000ull)|bits;
}
void PackedCells::fillZero(std::size_t cellCount){
  if(cellCount>data_.size())throw std::out_of_range("Screen clear size");
  std::fill(data_.begin(),data_.begin()+cellCount,0);
}
void PackedCells::fillZeroRange(std::size_t start,std::size_t end){
  if(start>data_.size())start=data_.size();
  if(end>data_.size())end=data_.size();
  if(start<end)std::fill(data_.begin()+start,data_.begin()+end,0);
}
void PackedCells::copyWithin(std::size_t destination,std::size_t start,
    std::size_t end){
  const std::vector<std::uint64_t> temporary(
    data_.begin()+start,data_.begin()+end);
  std::copy(temporary.begin(),temporary.end(),data_.begin()+destination);
}
void PackedCells::fillZeroJS(double start,double end){
  const auto first=typed_array::relativeIndex(start,data_.size());
  const auto last=typed_array::relativeIndex(end,data_.size());
  if(first<last)std::fill(data_.begin()+first,data_.begin()+last,0);
}
void PackedCells::copyWithinJS(double destination,double start,double end){
  typed_array::copyWithin(data_,destination,start,end);
}
void PackedCells::setWordsFromSubarray(const PackedCells& source,
    double begin,double end,double offset){
  const auto first=typed_array::relativeIndex(begin,source.length32());
  const auto last=typed_array::relativeIndex(end,source.length32());
  const auto count=last>first?last-first:std::size_t{0};
  const auto target=typed_array::toIntegerOrInfinity(offset);
  if(!std::isfinite(target)||target<0||
      target>static_cast<double>(length32())||
      count>length32()-static_cast<std::size_t>(target))
    throw std::range_error("offset is out of bounds");
  std::vector<std::int32_t> temporary;
  temporary.reserve(count);
  for(std::size_t i=first;i<last;i++)
    temporary.push_back(source.word(i).value_or(0));
  for(std::size_t i=0;i<count;i++)
    setWord(static_cast<std::int64_t>(target)+i,temporary[i]);
}
std::int64_t wordOffset(double cellIndex){
  // JavaScript's index << 1: ToInt32, shift, then signed Int32 result.
  const auto shifted=(static_cast<std::uint32_t>(
    typed_array::toInt32(cellIndex))<<1);
  return std::bit_cast<std::int32_t>(shifted);
}
std::int32_t packWord1(std::int32_t styleId,
    std::int32_t hyperlinkId,std::int32_t width){
  const auto bits=(static_cast<std::uint32_t>(styleId)<<17)|
    (static_cast<std::uint32_t>(hyperlinkId)<<2)|
    static_cast<std::uint32_t>(width);
  return std::bit_cast<std::int32_t>(bits);
}
} // namespace batbox::terminal_screen
