#include "cli/util/Ndjson.hpp"
#include <stdexcept>
namespace batbox::cli {
std::string escapeJsLineTerminators(const std::string& json){
  std::string escaped;
  escaped.reserve(json.size());
  for(std::size_t i=0;i<json.size();++i){
    if(i+2<json.size()&&static_cast<unsigned char>(json[i])==0xe2&&
       static_cast<unsigned char>(json[i+1])==0x80&&
       (static_cast<unsigned char>(json[i+2])==0xa8||
        static_cast<unsigned char>(json[i+2])==0xa9)){
      escaped+=static_cast<unsigned char>(json[i+2])==0xa8?"\\u2028":"\\u2029";
      i+=2;
    }else escaped+=json[i];
  }
  return escaped;
}
std::string ndjsonSafeStringify(const Json& value,const NdjsonStringifier& stringify){
  if(!stringify)throw std::invalid_argument("NDJSON jsonStringify import required");
  const auto json=stringify(value);
  if(!json)throw std::runtime_error("Cannot read properties of undefined (reading 'replace')");
  return escapeJsLineTerminators(*json);
}
}
