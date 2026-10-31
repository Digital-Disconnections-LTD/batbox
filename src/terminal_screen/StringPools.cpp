#include "terminal_screen/StringPools.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>

namespace batbox::terminal_screen {
CharPool::CharPool():strings_{" ",""},
    stringMap_{{" ",0},{"",1}}{
  ascii_.fill(-1);
  ascii_[32]=0;
}
std::int32_t CharPool::intern(const std::string& character){
  if(value::stringLength(character)==1){
    const auto units=value::utf16CodeUnits(character);
    const auto code=units[0];
    if(code<128){
      const auto stashed=ascii_[code];
      if(stashed!=-1)return stashed;
      const auto index=static_cast<std::int32_t>(strings_.size());
      strings_.push_back(character);
      ascii_[code]=index;
      return index;
    }
  }
  if(const auto existing=stringMap_.find(character);
      existing!=stringMap_.end())return existing->second;
  const auto index=static_cast<std::int32_t>(strings_.size());
  strings_.push_back(character);
  stringMap_.emplace(character,index);
  return index;
}
std::string CharPool::get(double index)const{
  if(!std::isfinite(index)||index<0||std::trunc(index)!=index||
      index>=static_cast<double>(strings_.size()))return " ";
  return strings_[static_cast<std::size_t>(index)];
}
HyperlinkPool::HyperlinkPool():strings_{""}{}
std::int32_t HyperlinkPool::intern(
    const std::optional<std::string>& hyperlink){
  if(!hyperlink||hyperlink->empty())return 0;
  if(const auto existing=stringMap_.find(*hyperlink);
      existing!=stringMap_.end())return existing->second;
  const auto index=static_cast<std::int32_t>(strings_.size());
  strings_.push_back(*hyperlink);
  stringMap_.emplace(*hyperlink,index);
  return index;
}
std::optional<std::string> HyperlinkPool::get(double id)const{
  if(id==0||!std::isfinite(id)||id<0||std::trunc(id)!=id||
      id>=static_cast<double>(strings_.size()))return {};
  return strings_[static_cast<std::size_t>(id)];
}
} // namespace batbox::terminal_screen
