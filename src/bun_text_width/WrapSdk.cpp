#include "bun_text_width/Wrap.hpp"
#include "bun_text_width/WrapInternal.hpp"
#include "runtime/ArrayJoin.hpp"
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace batbox::bun_text_width {
namespace {
std::string toJsString(const std::optional<Json>& input){
  if(!input||input->is_discarded())return "undefined";
  const Json& value=*input;
  if(value.is_string())return value.get<std::string>();
  if(value.is_null())return "null";
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_array())return value::arrayJoin(value);
  if(value.is_object()){
    if(value.contains("toString"))
      throw std::invalid_argument("No default value");
    return "[object Object]";
  }
  throw std::invalid_argument("Unsupported JavaScript value");
}
double numericString(const std::string& raw){
  const std::string text=value::trim(raw);
  if(text.empty())return 0;
  if(text=="Infinity"||text=="+Infinity")return std::numeric_limits<double>::infinity();
  if(text=="-Infinity")return -std::numeric_limits<double>::infinity();
  if(text.size()>2&&text[0]=='0'&&(text[1]=='b'||text[1]=='B'||
                                 text[1]=='o'||text[1]=='O')){
    const int base=text[1]=='b'||text[1]=='B'?2:8;
    double value=0;
    for(std::size_t i=2;i<text.size();++i){
      if(text[i]<'0'||text[i]>='0'+base)return std::numeric_limits<double>::quiet_NaN();
      value=value*base+(text[i]-'0');
    }
    return text.size()==2?std::numeric_limits<double>::quiet_NaN():value;
  }
  char* end=nullptr;
  const double value=std::strtod(text.c_str(),&end);
  return end==text.c_str()+text.size()?value:std::numeric_limits<double>::quiet_NaN();
}
double toNumber(const std::optional<Json>& input){
  if(!input||input->is_discarded())return std::numeric_limits<double>::quiet_NaN();
  const Json& value=*input;
  if(value.is_null())return 0;
  if(value.is_number())return value.get<double>();
  if(value.is_boolean())return value.get<bool>()?1:0;
  return numericString(toJsString(input));
}
std::size_t columnsFrom(const std::optional<Json>& input){
  if(!input||input->is_discarded())return 0;
  const double number=toNumber(input);
  if(!(number>0)||!std::isfinite(number))return 0;
  const double integer=std::trunc(number);
  if(integer>=static_cast<double>(std::numeric_limits<std::size_t>::max()))
    return std::numeric_limits<std::size_t>::max();
  return static_cast<std::size_t>(integer);
}
WrapOptions parseOptions(const std::optional<Json>& input){
  WrapOptions options;
  if(!input||!input->is_object())return options;
  const Json& value=*input;
  if(value.contains("hard"))options.hard=value::truthy(value.at("hard"));
  if(value.contains("wordWrap"))options.wordWrap=value::truthy(value.at("wordWrap"));
  if(value.contains("trim"))options.trim=value::truthy(value.at("trim"));
  if(value.contains("ambiguousIsNarrow"))
    options.ambiguousIsNarrow=value::truthy(value.at("ambiguousIsNarrow"));
  return options;
}
}
std::string wrapAnsiJs(const std::optional<Json>& input,
  const std::optional<Json>& columns,const std::optional<Json>& options){
  const std::string text=toJsString(input);
  // The SDK binding returns before column or option access for empty input.
  if(text.empty())return {};
  const auto width=columnsFrom(columns);
  const auto parsed=parseOptions(options);
  const auto units=value::utf16CodeUnits(text);
  bool latin1=true;
  for(const auto unit:units)if(unit>0xff){latin1=false;break;}
  return value::fromUtf16(wrapUnits(units,WrapContext{width,latin1,parsed}));
}
text_width::WrapAnsiRef indigenousWrapAnsiRef(){
  return std::make_shared<text_width::WrapAnsiFunction>(
    [](const std::string& text,double columns,const std::optional<Json>& options){
      return wrapAnsiJs(Json(text),Json(columns),options);
    });
}
} // namespace batbox::bun_text_width
