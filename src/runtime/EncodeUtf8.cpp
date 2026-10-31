#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
namespace batbox::value {
std::string encodeUtf8(std::string_view text){
  auto units=utf16CodeUnits(text);
  for(std::size_t i=0;i<units.size();++i){
    if(units[i]>=0xd800&&units[i]<=0xdbff){
      if(i+1<units.size()&&units[i+1]>=0xdc00&&units[i+1]<=0xdfff){++i;continue;}
      units[i]=0xfffd;
    }else if(units[i]>=0xdc00&&units[i]<=0xdfff)units[i]=0xfffd;
  }
  return fromUtf16(units);
}
}
