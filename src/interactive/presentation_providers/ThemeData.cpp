#include "interactive/presentation_providers/ThemeData.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <regex>

namespace batbox::interactive::presentation_providers {
namespace {
const std::array<std::string,6> names={"dark","light","light-daltonized",
  "dark-daltonized","light-ansi","dark-ansi"};
std::optional<int> decimal(std::string_view value){
  int result=0;
  const auto [end,error]=std::from_chars(value.data(),
    value.data()+value.size(),result);
  return error==std::errc{}&&end==value.data()+value.size()?result:
    std::optional<int>{};
}
std::optional<int> hex(std::string_view value){
  int result=0;
  const auto [end,error]=std::from_chars(value.data(),
    value.data()+value.size(),result,16);
  return error==std::errc{}&&end==value.data()+value.size()?result:
    std::optional<int>{};
}
} // namespace

const RawTheme& getTheme(std::string_view name){
  static const RawTheme dark=darkTheme(),light=lightTheme(),
    lightAnsi=lightAnsiTheme(),darkAnsi=darkAnsiTheme(),
    lightDaltonized=lightDaltonizedTheme(),
    darkDaltonized=darkDaltonizedTheme();
  static const RawTheme halloween=[] {
    auto theme=darkTheme();
    for(const auto* key:{"text","background","mascotBackground",
        "userMessageBackground","userMessageBackgroundHover",
        "bashMessageBackgroundColor","memoryBackgroundColor"})theme[key]="default";
    theme["brand"]="rgb(242,153,74)";
    theme["brandShimmer"]="rgb(255,185,120)";
    theme["suggestion"]=theme["remember"]=theme["success"]=
      theme["brandBlueSpinner"]="rgb(151,204,105)";
    theme["permission"]=theme["bashBorder"]=theme["warning"]=
      theme["briefLabelAssistant"]="rgb(242,153,74)";
    theme["planMode"]="rgb(151,204,105)";
    theme["subtle"]="rgb(156,156,156)";
    theme["inactive"]="rgb(176,176,176)";
    theme["promptBorder"]="rgb(110,110,110)";
    return theme;
  }();
  static const RawTheme halloweenAnsi=[] {
    auto theme=darkAnsiTheme();
    for(const auto* key:{"text","background","mascotBackground",
        "userMessageBackground","userMessageBackgroundHover",
        "bashMessageBackgroundColor","memoryBackgroundColor"})theme[key]="default";
    for(const auto* key:{"brand","brandShimmer","permission","bashBorder",
        "warning","briefLabelAssistant"})theme[key]="ansi:yellow";
    for(const auto* key:{"suggestion","remember","success","planMode",
        "brandBlueSpinner"})theme[key]="ansi:greenBright";
    return theme;
  }();
  static const RawTheme monochrome=[] {
    auto theme=darkTheme();for(auto& [key,value]:theme)value="default";return theme;
  }();
  if(name=="halloween")return halloween;
  if(name=="halloween-ansi")return halloweenAnsi;
  if(name=="monochrome")return monochrome;
  if(name=="light")return light;
  if(name=="light-ansi")return lightAnsi;
  if(name=="dark-ansi")return darkAnsi;
  if(name=="light-daltonized")return lightDaltonized;
  if(name=="dark-daltonized")return darkDaltonized;
  return dark;
}
std::vector<std::string> themeNames(){return {names.begin(),names.end()};}
std::string resolveThemeName(std::string_view setting,
    std::string_view systemTheme){
  return setting=="auto"?std::string(systemTheme):std::string(setting);
}
std::optional<std::string> rawColor(std::string_view theme,
    std::string_view key){
  const auto& palette=getTheme(theme);
  const auto found=palette.find(std::string(key));
  return found==palette.end()?std::optional<std::string>{}:found->second;
}
std::optional<ftxui::Color> parseColor(std::string_view raw){
  if(raw.starts_with("ansi:")){
    static const std::unordered_map<std::string,int> indexed={
      {"black",0},{"red",1},{"green",2},{"yellow",3},{"blue",4},
      {"magenta",5},{"cyan",6},{"white",7},{"blackBright",8},
      {"redBright",9},{"greenBright",10},{"yellowBright",11},
      {"blueBright",12},{"magentaBright",13},{"cyanBright",14},
      {"whiteBright",15}};
    const auto found=indexed.find(std::string(raw.substr(5)));
    if(found==indexed.end())return {};
    return ftxui::Color(static_cast<ftxui::Color::Palette16>(found->second));
  }
  static const std::regex rgb(R"(^rgb\(\s?(\d+),\s?(\d+),\s?(\d+)\s?\)$)");
  std::cmatch match;
  const std::string text(raw);
  if(std::regex_match(text.c_str(),match,rgb)){
    const auto r=decimal(match[1].str()),g=decimal(match[2].str()),
      b=decimal(match[3].str());
    if(!r||!g||!b)return {};
    return ftxui::Color::RGB(std::clamp(*r,0,255),
      std::clamp(*g,0,255),std::clamp(*b,0,255));
  }
  static const std::regex indexed256(R"(^ansi256\(\s?(\d+)\s?\)$)");
  if(std::regex_match(text.c_str(),match,indexed256)){
    const auto value=decimal(match[1].str());
    if(!value)return {};
    return ftxui::Color(static_cast<ftxui::Color::Palette256>(
      std::clamp(*value,0,255)));
  }
  if(raw.starts_with('#')){
    auto digits=raw.substr(1);
    if(digits.size()==3){
      std::string expanded;
      for(const auto ch:digits){expanded+=ch;expanded+=ch;}
      digits=std::string_view(expanded);
      const auto value=hex(digits);
      if(!value)return {};
      return ftxui::Color::RGB((*value>>16)&255,(*value>>8)&255,*value&255);
    }
    if(digits.size()==6){
      const auto value=hex(digits);
      if(!value)return {};
      return ftxui::Color::RGB((*value>>16)&255,(*value>>8)&255,*value&255);
    }
  }
  return {};
}
ftxui::Color themeColor(std::string_view theme,std::string_view key){
  const auto raw=rawColor(theme,key);
  if(!raw)return ftxui::Color::Default;
  return parseColor(*raw).value_or(ftxui::Color::Default);
}
} // namespace batbox::interactive::presentation_providers
