#include "terminal_color/Colorize.hpp"
#include <array>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace batbox::terminal_color {
namespace {
bool whitespace(char16_t c){
  return (c>=0x09&&c<=0x0D)||c==0x20||c==0xA0||c==0x1680||
    (c>=0x2000&&c<=0x200A)||c==0x2028||c==0x2029||c==0x202F||
    c==0x205F||c==0x3000||c==0xFEFF;
}
std::optional<std::vector<double>> numericColor(const std::string& color,
    const std::string& prefix,std::size_t count){
  const auto input=value::utf16CodeUnits(color);
  std::size_t i=0;
  for(const auto c:prefix){
    if(i>=input.size()||input[i++]!=static_cast<char16_t>(c))return {};
  }
  if(i>=input.size()||input[i++]!=u'(')return {};
  std::vector<double> values;values.reserve(count);
  for(std::size_t j=0;j<count;j++){
    if(i<input.size()&&whitespace(input[i]))i++;
    const auto begin=i;
    while(i<input.size()&&input[i]>=u'0'&&input[i]<=u'9')i++;
    if(begin==i)return {};
    std::string decimal;
    for(auto at=begin;at<i;at++)decimal+=static_cast<char>(input[at]);
    values.push_back(std::strtod(decimal.c_str(),nullptr));
    if(j+1<count){
      if(i>=input.size()||input[i++]!=u',')return {};
    }
  }
  if(i<input.size()&&whitespace(input[i]))i++;
  if(i>=input.size()||input[i++]!=u')'||i!=input.size())return {};
  return values;
}
std::optional<std::string> stringColor(const Json& styles,
    const char* name){
  if(!styles.is_object())throw std::invalid_argument("TextStyles required");
  const auto found=styles.find(name);
  if(found==styles.end()||!value::truthy(*found))return {};
  if(!found->is_string())throw std::invalid_argument(
    std::string("Color string required: ")+name);
  return found->get<std::string>();
}
bool flag(const Json& styles,const char* name){
  if(!styles.is_object())throw std::invalid_argument("TextStyles required");
  const auto found=styles.find(name);
  return found!=styles.end()&&value::truthy(*found);
}
}
std::string colorize(const std::string& text,
    const std::optional<std::string>& color,ColorType type,
    const ChalkState& chalk){
  if(!color||color->empty())return text;
  const auto background=type==ColorType::Background;
  if(color->starts_with("ansi:")){
    static constexpr std::array<const char*,16> names{{
      "black","red","green","yellow","blue","magenta","cyan",
      "white","blackBright","redBright","greenBright","yellowBright",
      "blueBright","magentaBright","cyanBright","whiteBright"}};
    const auto value=color->substr(5);
    for(const auto name:names){
      if(value==name)
        return chalkNamed(text,background?"bg"+
          std::string(1,static_cast<char>(name[0]-32))+
          std::string(name+1):value,chalk.level);
    }
  }
  if(color->starts_with("#"))
    return chalkHex(text,*color,background,chalk.level);
  if(color->starts_with("ansi256")){
    const auto values=numericColor(*color,"ansi256",1);
    if(!values)return text;
    return chalkAnsi256(text,values->at(0),background,chalk.level);
  }
  if(color->starts_with("rgb")){
    const auto values=numericColor(*color,"rgb",3);
    if(!values)return text;
    return chalkRgb(text,values->at(0),values->at(1),values->at(2),
      background,chalk.level);
  }
  return text;
}
std::string applyTextStyles(const std::string& text,
    const terminal_render::StylesRef& styles,
    const ChalkState& chalk){
  if(!styles)throw std::invalid_argument("TextStyles required");
  auto result=text;
  for(const char* modifier:{"inverse","strikethrough","underline",
      "italic","bold","dim"}){
    if(flag(*styles,modifier))
      result=chalkNamed(result,modifier,chalk.level);
  }
  if(const auto color=stringColor(*styles,"color"))
    result=colorize(result,color,ColorType::Foreground,chalk);
  if(const auto color=stringColor(*styles,"backgroundColor"))
    result=colorize(result,color,ColorType::Background,chalk);
  return result;
}
std::string applyColor(const std::string& text,
    const std::optional<std::string>& color,
    const ChalkState& chalk){
  if(!color||color->empty())return text;
  return colorize(text,color,ColorType::Foreground,chalk);
}
terminal_render::BorderImports bindBorderColor(const ChalkState& chalk,
    std::function<double(const std::string&)> stringWidth){
  terminal_render::BorderImports imports;
  imports.stringWidth=std::move(stringWidth);
  imports.applyColor=[chalk](const std::string& text,
      const std::optional<std::string>& color){
    return applyColor(text,color,chalk);
  };
  imports.dim=[chalk](const std::string& text){
    return chalkNamed(text,"dim",chalk.level);
  };
  imports.resolveBox=terminal_render::selectedCliBox;
  return imports;
}
} // namespace batbox::terminal_color
