#include "runtime/PathSanitization.hpp"
#include "runtime/JsonValue.hpp"
#include "utils/hash.hpp"
#include <algorithm>

namespace batbox::runtime {
namespace {
std::string base36(std::uint64_t value){
  if(!value)return "0";
  std::string text;
  while(value){const auto digit=value%36;text+=static_cast<char>(digit<10?'0'+digit:'a'+digit-10);value/=36;}
  std::reverse(text.begin(),text.end());return text;
}
std::string simpleHash(const std::string& name){
  const std::int64_t signedValue=utils::djb2Hash(name);
  return base36(static_cast<std::uint64_t>(signedValue<0?-signedValue:signedValue));
}
}
std::string sanitizePath(const std::string& name,
  const std::function<std::string(const std::string&)>& longPathHash){
  std::string sanitized;
  for(const auto unit:value::utf16CodeUnits(name))sanitized+=
    ((unit>='a'&&unit<='z')||(unit>='A'&&unit<='Z')||(unit>='0'&&unit<='9'))?static_cast<char>(unit):'-';
  // Leading separators collapse away: an absolute path's root marker must
  // not become a leading dash in the slug (a "/x" dir slugs as "x", not "-x").
  const auto first=sanitized.find_first_not_of('-');
  if(first==std::string::npos)return "root";
  sanitized.erase(0,first);
  if(sanitized.size()<=MAX_SANITIZED_LENGTH)return sanitized;
  std::string hash;
  if(longPathHash)hash=longPathHash(name);
  else hash=simpleHash(name);
  return sanitized.substr(0,MAX_SANITIZED_LENGTH)+"-"+hash;
}
}
