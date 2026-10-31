#include "ansi_tokenize/AnsiTokenize.hpp"
#include "runtime/ArrayJoin.hpp"
#include <cstdlib>
#include <unordered_set>

namespace batbox::ansi_tokenize {
namespace {
std::string sgr(int code){return "\x1b["+std::to_string(code)+"m";}
bool starts(const std::string& text,const std::string& prefix){return text.rfind(prefix,0)==0;}
bool ends(const std::string& text,const std::string& suffix){
  return text.size()>=suffix.size()&&text.compare(text.size()-suffix.size(),suffix.size(),suffix)==0;
}
int closeCode(int start){
  if(start==0)return 0;
  if(start==1||start==2)return 22;
  if(start==3)return 23;
  if(start==4)return 24;
  if(start==53)return 55;
  if(start==7)return 27;
  if(start==8)return 28;
  if(start==9)return 29;
  if((start>=30&&start<=37)||(start>=90&&start<=97))return 39;
  if((start>=40&&start<=47)||(start>=100&&start<=107))return 49;
  return -1;
}
bool endCode(const std::string& code){
  for(const int value:{0,22,23,24,55,27,28,29,39,49})
    if(code==sgr(value))return true;
  return false;
}
std::string jsText(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_null())return "null";
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_array())return value::arrayJoin(value);
  return "[object Object]";
}
}
std::string getEndCode(const std::string& code){
  if(endCode(code))return code;
  for(const int start:{0,1,2,3,4,53,7,8,9,30,31,32,33,34,35,36,37,
                       90,91,92,93,94,95,96,97,40,41,42,43,44,45,46,47,
                       100,101,102,103,104,105,106,107})
    if(code==sgr(start))return sgr(closeCode(start));
  if(starts(code,"\x1b]8;")){
    if(ends(code,"\x1b\\"))return "\x1b]8;;\x1b\\";
    if(ends(code,"\xc2\x9c"))return "\x1b]8;;\xc2\x9c";
    return "\x1b]8;;\x07";
  }
  const auto units=value::utf16CodeUnits(code);
  const auto rest=units.size()>=2?value::fromUtf16(std::u16string_view(units).substr(2)):
    std::string{};
  if(starts(rest,"38"))return sgr(39);
  if(starts(rest,"48"))return sgr(49);
  char* end=nullptr;
  const long value=std::strtol(rest.c_str(),&end,10);
  if(end!=rest.c_str()){
    const int close=closeCode(static_cast<int>(value));
    if(close>=0)return sgr(close);
  }
  return sgr(0);
}
std::string getLinkStartCode(const std::string& url,const Json& params){
  std::string keys;
  if(value::truthy(params)&&params.is_object()){
    for(auto it=params.begin();it!=params.end();++it){
      if(!keys.empty())keys+=':';
      keys+=it.key()+"="+jsText(it.value());
    }
  }
  return "\x1b]8;"+keys+";"+url+"\x07";
}
std::string ansiCodesToString(const Codes& codes){
  std::string result;
  std::unordered_set<std::string> seen;
  for(const auto& code:codes)if(code&&seen.insert(code->code).second)result+=code->code;
  return result;
}
bool isIntensityCode(const AnsiCode& code){
  return code.code=="\x1b[1m"||code.code=="\x1b[2m";
}
} // namespace batbox::ansi_tokenize
