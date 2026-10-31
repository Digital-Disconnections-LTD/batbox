#include "terminal_output/BidiTypes.hpp"
#include "terminal_output/BidiData.generated.hpp"
#include <algorithm>

namespace batbox::terminal_output {
std::uint32_t bidiCharType(char16_t character){
  const auto code=static_cast<std::uint32_t>(character);
  const auto& ranges=bidi_data::charTypes;
  auto first=ranges.begin(),last=ranges.end();
  while(first<last){
    const auto middle=first+(last-first)/2;
    if(middle->start<=code)first=middle+1;
    else last=middle;
  }
  if(first==ranges.begin())return bidi_data::L;
  const auto& range=*(first-1);
  return code<=range.end?range.type:bidi_data::L;
}
namespace {
template<class Pairs> std::optional<std::uint32_t> lookup(
    const Pairs& pairs,std::optional<std::uint32_t> character){
  if(!character)return {};
  auto first=pairs.begin(),last=pairs.end();
  while(first<last){
    const auto middle=first+(last-first)/2;
    if(middle->from<*character)first=middle+1;
    else last=middle;
  }
  if(first==pairs.end()||first->from!=*character)return {};
  return first->to;
}
}
std::optional<std::uint32_t> openingToClosingBracket(
    std::optional<std::uint32_t> character){
  return lookup(bidi_data::openToClose,character);
}
std::optional<std::uint32_t> closingToOpeningBracket(
    std::optional<std::uint32_t> character){
  return lookup(bidi_data::closeToOpen,character);
}
std::optional<std::uint32_t> canonicalBracket(
    std::optional<std::uint32_t> character){
  return lookup(bidi_data::canonical,character);
}
} // namespace batbox::terminal_output
