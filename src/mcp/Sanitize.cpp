#include "mcp/Mcp.hpp"
#include "runtime/JsonValue.hpp"
#include <unicode/uchar.h>
#include <unicode/unorm2.h>
#include <stdexcept>

namespace batbox::mcp {
namespace {
std::u16string normalizeNfkc(const std::u16string& input){
  UErrorCode error=U_ZERO_ERROR;
  const auto* nfkc=unorm2_getNFKCInstance(&error);
  if(U_FAILURE(error))throw std::runtime_error("NFKC normalizer unavailable");
  error=U_ZERO_ERROR;
  const auto length=unorm2_normalize(nfkc,reinterpret_cast<const UChar*>(input.data()),
    static_cast<int32_t>(input.size()),nullptr,0,&error);
  if(error!=U_BUFFER_OVERFLOW_ERROR&&U_FAILURE(error))
    throw std::runtime_error("NFKC normalization failed");
  std::u16string result(static_cast<std::size_t>(length),u'\0');
  error=U_ZERO_ERROR;
  const auto produced=unorm2_normalize(nfkc,reinterpret_cast<const UChar*>(input.data()),
    static_cast<int32_t>(input.size()),reinterpret_cast<UChar*>(result.data()),length,&error);
  if(U_FAILURE(error))throw std::runtime_error("NFKC normalization failed");
  result.resize(static_cast<std::size_t>(produced));
  return result;
}
bool explicitStrip(UChar32 cp){
  return (cp>=0x200b&&cp<=0x200f)||(cp>=0x202a&&cp<=0x202e)||
    (cp>=0x2066&&cp<=0x2069)||cp==0xfeff||(cp>=0xe000&&cp<=0xf8ff);
}
std::u16string removeHidden(const std::u16string& text){
  std::u16string output;
  for(std::size_t i=0;i<text.size();++i){
    UChar32 cp=text[i];
    const bool pair=cp>=0xd800&&cp<=0xdbff&&i+1<text.size()&&
      text[i+1]>=0xdc00&&text[i+1]<=0xdfff;
    if(pair)cp=0x10000+((cp-0xd800)<<10)+(text[i+1]-0xdc00);
    const auto category=u_charType(cp);
    if(category!=U_FORMAT_CHAR&&category!=U_PRIVATE_USE_CHAR&&
       category!=U_UNASSIGNED&&!explicitStrip(cp)){
      output+=text[i];
      if(pair)output+=text[i+1];
    }
    if(pair)++i;
  }
  return output;
}
}
std::string partiallySanitizeUnicode(const std::string& poetry){
  std::string current=poetry,previous;
  int iterations=0;
  while(current!=previous&&iterations<10){
    previous=current;
    current=value::fromUtf16(removeHidden(normalizeNfkc(value::utf16CodeUnits(current))));
    ++iterations;
  }
  if(iterations>=10)throw std::runtime_error(
    "Unicode sanitization reached maximum iterations (10) for input: "+
    value::slice(poetry,0,100));
  return current;
}
Json recursivelySanitizeUnicode(const Json& value){
  if(value.is_string())return partiallySanitizeUnicode(value.get<std::string>());
  if(value.is_array()){
    Json output=Json::array();
    for(const auto& item:value)output.push_back(recursivelySanitizeUnicode(item));
    return output;
  }
  if(value.is_object()){
    Json output=Json::object();
    for(const auto& key:jsOwnKeys(value))
      output[partiallySanitizeUnicode(key)]=recursivelySanitizeUnicode(value.at(key));
    return output;
  }
  return value;
}
} // namespace batbox::mcp
