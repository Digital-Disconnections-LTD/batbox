#include "bun_text_width/Width.hpp"
#include "runtime/ArrayJoin.hpp"
#include <stdexcept>

namespace batbox::bun_text_width {
namespace {
std::string toJsString(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_null())return "null";
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_array())return value::arrayJoin(value);
  if(value.is_object()){
    if(value.contains("toString"))
      throw std::invalid_argument("Cannot convert object to primitive value");
    return "[object Object]";
  }
  throw std::invalid_argument("Unsupported JavaScript value");
}
}
double stringWidth(const std::optional<Json>& value,const Json& options){
  if(!value||value->is_discarded())return 0;
  const std::string text=toJsString(*value);
  if(text.empty())return 0;
  bool countAnsi=false,ambiguousIsNarrow=true;
  if(options.is_object()){
    if(options.contains("countAnsiEscapeCodes"))
      countAnsi=value::truthy(options.at("countAnsiEscapeCodes"));
    if(options.contains("ambiguousIsNarrow"))
      ambiguousIsNarrow=value::truthy(options.at("ambiguousIsNarrow"));
  }
  const std::u16string units=value::utf16CodeUnits(text);
  bool latin1=true;
  for(const auto unit:units)if(unit>0xff){latin1=false;break;}
  return latin1?widthLatin1(units,countAnsi):
    widthUtf16(units,countAnsi,ambiguousIsNarrow);
}
text_width::BunWidth indigenousWidthCallback(){
  return [](const Json& value,const Json& options){
    return stringWidth(value,options);
  };
}
} // namespace batbox::bun_text_width
