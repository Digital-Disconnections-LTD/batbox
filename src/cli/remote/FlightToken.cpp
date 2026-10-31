#include "cli/remote/FlightToken.hpp"
#include <string_view>

namespace batbox::cli::remote {
namespace {
std::string decodeBase64Url(std::string_view value){
  std::string decoded;
  unsigned int bits=0;
  int count=0;
  for(unsigned char ch:value){
    if(ch=='=')break;
    int digit=-1;
    if(ch>='A'&&ch<='Z')digit=ch-'A';
    else if(ch>='a'&&ch<='z')digit=ch-'a'+26;
    else if(ch>='0'&&ch<='9')digit=ch-'0'+52;
    else if(ch=='-'||ch=='+')digit=62;
    else if(ch=='_'||ch=='/')digit=63;
    if(digit<0)continue;
    bits=(bits<<6)|static_cast<unsigned int>(digit);
    count+=6;
    if(count>=8){
      count-=8;
      decoded+=static_cast<char>((bits>>count)&0xff);
    }
  }
  return decoded;
}
}
Json decodeFlightJwtPayload(const std::string& token,
    const std::string& configuredPrefix){
  const std::string_view jwt=token.starts_with(configuredPrefix)?
    std::string_view(token).substr(configuredPrefix.size()):token;
  const auto first=jwt.find('.');
  if(first==std::string_view::npos)return nullptr;
  const auto second=jwt.find('.',first+1);
  if(second==std::string_view::npos||second==first+1||
     jwt.find('.',second+1)!=std::string_view::npos)return nullptr;
  try{
    const auto bytes=decodeBase64Url(jwt.substr(first+1,second-first-1));
    auto parsed=value::parse(value::decodeUtf8(bytes),false);
    return parsed.is_discarded()?Json(nullptr):parsed;
  }catch(...){return nullptr;}
}
std::optional<double> decodeFlightJwtExpiry(const std::string& token,
    const std::string& configuredPrefix){
  const auto payload=decodeFlightJwtPayload(token,configuredPrefix);
  const auto& expiry=value::property(payload,"exp");
  if(payload.is_object()&&expiry.is_number())return expiry.get<double>();
  return {};
}
}
