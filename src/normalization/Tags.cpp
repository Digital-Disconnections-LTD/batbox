#include "normalization/Internal.hpp"
#include <algorithm>
#include <cstdint>

namespace batbox::normalization {
using namespace internal;

std::string deriveShortMessageId(const std::string& uuid) {
  std::string hex;
  for(char c:uuid) if(c!='-') {hex.push_back(c);if(hex.size()==10) break;}
  std::uint64_t value=0;bool found=false;
  for(char c:hex) {
    int digit=-1;
    if(c>='0'&&c<='9') digit=c-'0';
    else if(c>='a'&&c<='f') digit=c-'a'+10;
    else if(c>='A'&&c<='F') digit=c-'A'+10;
    if(digit<0) break;
    found=true;value=value*16+static_cast<std::uint64_t>(digit);
  }
  if(!found) return "NaN";
  if(value==0) return "0";
  std::string base36;
  while(value>0){int digit=static_cast<int>(value%36);base36.push_back(digit<10?char('0'+digit):char('a'+digit-10));value/=36;}
  std::reverse(base36.begin(),base36.end());
  return base36.substr(0,6);
}

Json appendMessageTagToUserMessage(const Json& message) {
  if(value::truthy(get(message,"isMeta"))) return message;
  const std::string tag="\n[id:"+deriveShortMessageId(get(message,"uuid").get<std::string>())+"]";
  const Json& content=get(get(message,"message"),"content");
  if(content.is_string()) {
    Json result=message;result["message"]["content"]=content.get<std::string>()+tag;return result;
  }
  if(!content.is_array()||content.empty()) return message;
  int lastTextIndex=-1;
  for(int i=static_cast<int>(content.size())-1;i>=0;--i)
    if(typeIs(content[static_cast<std::size_t>(i)],"text")){lastTextIndex=i;break;}
  if(lastTextIndex<0) return message;
  Json result=message;
  Json& block=result["message"]["content"][static_cast<std::size_t>(lastTextIndex)];
  block["text"]=get(block,"text").get<std::string>()+tag;
  return result;
}

} // namespace batbox::normalization
