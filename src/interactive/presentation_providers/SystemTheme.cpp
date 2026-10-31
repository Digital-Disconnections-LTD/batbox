#include "interactive/presentation_providers/SystemTheme.hpp"
#include "runtime/JsonValue.hpp"
#include <charconv>
#include <cmath>
#include <regex>
#include <stdexcept>

namespace batbox::interactive::presentation_providers {
namespace {
std::optional<double> component(const std::string& text){
  unsigned value=0;
  const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),
    value,16);
  if(error!=std::errc{}||end!=text.data()+text.size())return {};
  const auto max=std::pow(16.,static_cast<double>(text.size()))-1;
  return static_cast<double>(value)/max;
}
}
std::optional<std::string> themeFromOscColor(const std::string& data){
  static const std::regex rgb(R"(^rgba?:([0-9a-f]{1,4})/([0-9a-f]{1,4})/([0-9a-f]{1,4}))",
    std::regex_constants::icase);
  static const std::regex hex(R"(^#([0-9a-f]+)$)",
    std::regex_constants::icase);
  std::smatch match;
  std::optional<double> r,g,b;
  if(std::regex_search(data,match,rgb)){
    r=component(match[1].str());g=component(match[2].str());
    b=component(match[3].str());
  }else if(std::regex_match(data,match,hex)){
    const auto text=match[1].str();
    if(text.size()%3)return {};
    const auto count=text.size()/3;
    r=component(text.substr(0,count));
    g=component(text.substr(count,count));
    b=component(text.substr(count*2,count));
  }else return {};
  if(!r||!g||!b)return {};
  const auto luminance=0.2126*(*r)+0.7152*(*g)+0.0722*(*b);
  return luminance>0.5?"light":"dark";
}
std::optional<std::string> detectFromColorFgBg(const std::string& value){
  if(value.empty())return {};
  const auto pos=value.find_last_of(';');
  const auto bg=value.substr(pos==std::string::npos?0:pos+1);
  if(bg.empty())return {};
  const auto trimmed=value::trim(bg);
  if(trimmed.empty())return "dark";
  int number=0;
  const auto [end,error]=std::from_chars(trimmed.data(),
    trimmed.data()+trimmed.size(),number);
  if(error!=std::errc{}||end!=trimmed.data()+trimmed.size()||
     number<0||number>15)return {};
  return number<=6||number==8?"dark":"light";
}
SystemThemeStash::SystemThemeStash(
    std::function<std::optional<std::string>()> colorFgBg):
    colorFgBg_(std::move(colorFgBg)){
  if(!colorFgBg_)throw std::invalid_argument("COLORFGBG provider required");
}
std::string SystemThemeStash::current(){
  if(!stashed_)stashed_=detectFromColorFgBg(colorFgBg_().value_or(""))
    .value_or("dark");
  return *stashed_;
}
void SystemThemeStash::set(std::string theme){
  if(theme!="dark"&&theme!="light")
    throw std::invalid_argument("System theme must be dark or light");
  stashed_=std::move(theme);
}
} // namespace batbox::interactive::presentation_providers
