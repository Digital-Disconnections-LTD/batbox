#include "interactive/message_actions/MessageActions.hpp"
#include "runtime/JsonValue.hpp"
#include <string_view>

namespace batbox::interactive::message_actions {
namespace {
bool space(char16_t c){
  return (c>=9&&c<=13)||c==0x20||c==0xa0||c==0x1680||
    (c>=0x2000&&c<=0x200a)||c==0x2028||c==0x2029||
    c==0x202f||c==0x205f||c==0x3000||c==0xfeff;
}
std::string trimStart(const std::string& text){
  const auto units=value::utf16CodeUnits(text);
  std::size_t start=0;
  while(start<units.size()&&space(units[start]))++start;
  return value::fromUtf16(std::u16string_view(units).substr(start));
}
} // namespace

std::string stripSystemReminders(const std::string& text){
  constexpr std::string_view open="<system-reminder>";
  constexpr std::string_view close="</system-reminder>";
  auto rest=trimStart(text);
  while(rest.starts_with(open)){
    const auto end=rest.find(close);
    if(end==std::string::npos)break;
    rest=trimStart(rest.substr(end+close.size()));
  }
  return rest;
}
} // namespace batbox::interactive::message_actions
