#include "termio/OscLink.hpp"
#include "runtime/ArrayJoin.hpp"
#include <cstdint>
namespace batbox::termio {
std::string osc(const Json& parts,const std::string& terminal){
  const auto terminator=terminal=="kitty"?"\x1b\\":"\x07";
  return std::string("\x1b]")+value::arrayJoin(parts,";")+terminator;
}
std::string osc8Id(const std::string& url){
  std::uint32_t h=0;
  for(const auto code:value::utf16CodeUnits(url))h=h*31+code;
  const char* digits="0123456789abcdefghijklmnopqrstuvwxyz";
  std::string result;
  do{result.insert(result.begin(),digits[h%36]);h/=36;}while(h);
  return result;
}
std::string OscModule::link(const std::string& url,const std::optional<Json>& params)const{
  if(url.empty())return linkEnd_;
  Json p={{"id",osc8Id(url)}};
  if(params)for(auto it=params->begin();it!=params->end();++it)p[it.key()]=it.value();
  std::string paramString;
  for(const auto& key:value::objectKeys(p)){
    if(!paramString.empty())paramString+=':';
    paramString+=key+"="+p.at(key).get<std::string>();
  }
  return osc(Json::array({8,paramString,url}),terminal_());
}
}
