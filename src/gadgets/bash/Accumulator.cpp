#include "gadgets/bash/Accumulator.hpp"
#include "runtime/JsonValue.hpp"
#include "shell/ManagedShellInternal.hpp"
#include <algorithm>
#include <cmath>

namespace batbox::bash_gadget {
void EndTruncatingAccumulator::append(std::string_view text){
  const auto units=value::utf16CodeUnits(std::string(text));
  totalBytesReceived_+=units.size();
  const auto current=value::utf16CodeUnits(content_);
  if(isTruncated_&&static_cast<double>(current.size())>=maxSize_)return;
  if(static_cast<double>(current.size()+units.size())>maxSize_){
    const auto room=maxSize_-static_cast<double>(current.size());
    if(room>0){
      const auto count=std::min(units.size(),static_cast<std::size_t>(room));
      content_+=value::fromUtf16(std::u16string_view(units.data(),count));
    }
    isTruncated_=true;
  }else content_+=text;
}
void EndTruncatingAccumulator::appendBuffer(std::string_view utf8Bytes){
  append(shell::utf8WithReplacement(utf8Bytes));
}
std::string EndTruncatingAccumulator::toString()const{
  if(!isTruncated_)return content_;
  const auto removed=static_cast<double>(totalBytesReceived_)-maxSize_;
  const auto kilobytes=std::floor(removed/1024+0.5);
  return content_+"\n... [output truncated - "+value::numberToString(kilobytes)+"KB removed]";
}
void EndTruncatingAccumulator::clear(){content_.clear();isTruncated_=false;totalBytesReceived_=0;}
std::size_t EndTruncatingAccumulator::length()const{return value::utf16CodeUnits(content_).size();}
} // namespace batbox::bash_gadget
