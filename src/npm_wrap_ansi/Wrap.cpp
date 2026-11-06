#include "npm_wrap_ansi/Wrap.hpp"
#include "npm_wrap_ansi/Internal.hpp"
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
text_width::WrapAnsiRef indigenousWrapAnsiRef(){
  return std::make_shared<text_width::WrapAnsiFunction>(
    [](const std::string& input,double columns,const std::optional<Json>& options){
      return wrapAnsi(input,columns,options);
    });
}
}
