#include "npm_wrap_ansi/Wrap.hpp"
#include "npm_wrap_ansi/Internal.hpp"
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace batbox::npm_wrap_ansi {
namespace {
Options readOptions(const std::optional<Json>& value){
  Options result;
  if(!value||value->is_discarded())return result;
  if(value->is_null())throw std::runtime_error("Cannot read properties of null (reading 'trim')");
  if(!value->is_object())return result;
  auto presentFalse=[&](const char* key){
    const auto found=value->find(key);return found!=value->end()&&found->is_boolean()&&!found->get<bool>();
  };
  result.trim=!presentFalse("trim");result.wordWrap=!presentFalse("wordWrap");
  const auto found=value->find("hard");
  result.hard=found!=value->end()&&value::truthy(*found);
  return result;
}
std::string jsString(const Json& value){
  if(value.is_discarded())return "undefined";
  if(value.is_null())return "null";
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_string())return value.get<std::string>();
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_array()){
    std::string result;
    for(std::size_t i=0;i<value.size();++i){
      if(i)result+=',';
      if(!value[i].is_null()&&!value[i].is_discarded())result+=jsString(value[i]);
    }
    return result;
  }
  return "[object Object]";
}
double jsNumber(const Json& value){
  if(value.is_discarded())return std::numeric_limits<double>::quiet_NaN();
  if(value.is_null())return 0;
  if(value.is_boolean())return value.get<bool>()?1:0;
  if(value.is_number())return value.get<double>();
  const auto text=value::trim(jsString(value));
  if(text.empty())return 0;
  if(text.size()>2&&text[0]=='0'&&
     (text[1]=='b'||text[1]=='B'||text[1]=='o'||text[1]=='O')){
    const int base=(text[1]=='b'||text[1]=='B')?2:8;
    double number=0;
    for(std::size_t i=2;i<text.size();++i){
      if(text[i]<'0'||text[i]>='0'+base)return std::numeric_limits<double>::quiet_NaN();
      number=number*base+(text[i]-'0');
    }
    return number;
  }
  char* end=nullptr;const double number=std::strtod(text.c_str(),&end);
  return end==text.c_str()+text.size()?number:std::numeric_limits<double>::quiet_NaN();
}
}
std::string wrapAnsi(const std::string& input,double columns,const std::optional<Json>& options){
  // String(input).normalize() occurs before options access in the SDK.
  auto units=normalize(value::utf16CodeUnits(input));
  std::u16string crlf;
  crlf.reserve(units.size());
  for(std::size_t i=0;i<units.size();++i){
    if(units[i]==u'\r'&&i+1<units.size()&&units[i+1]==u'\n'){
      crlf+=u'\n';++i;
    }else crlf+=units[i];
  }
  std::u16string result;std::size_t start=0;
  while(start<=crlf.size()){
    const auto end=crlf.find(u'\n',start);
    const auto finish=end==std::u16string::npos?crlf.size():end;
    const auto settings=readOptions(options);
    result+=wrapLine(std::u16string_view(crlf).substr(start,finish-start),columns,settings);
    if(end==std::u16string::npos)break;
    result+=u'\n';start=end+1;
  }
  return value::fromUtf16(result);
}
std::string wrapAnsiJs(const Json& input,const Json& columns,const std::optional<Json>& options){
  // The JSON bridge performs String(input), then numeric column conversion.
  // The typed application seam already receives a numeric width.
  return wrapAnsi(jsString(input),jsNumber(columns),options);
}
text_width::WrapAnsiRef indigenousWrapAnsiRef(){
  return std::make_shared<text_width::WrapAnsiFunction>(
    [](const std::string& input,double columns,const std::optional<Json>& options){
      return wrapAnsi(input,columns,options);
    });
}
}
