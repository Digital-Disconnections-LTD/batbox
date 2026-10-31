#include "settings/remote/Checksum.hpp"
#include "runtime/Encoding.hpp"
#include <algorithm>
#include <array>
#include <iomanip>
#include <openssl/sha.h>
#include <sstream>
#include <vector>

namespace batbox::settings::remote {
Json sortKeysDeep(const Json& value){
  if(value.is_array()){
    Json sorted=Json::array();
    for(const auto& item:value)sorted.push_back(sortKeysDeep(item));
    return sorted;
  }
  if(value.is_object()){
    auto keys=value::objectKeys(value);
    std::sort(keys.begin(),keys.end(),[](const std::string& a,
        const std::string& b){
      return value::utf16CodeUnits(a)<value::utf16CodeUnits(b);
    });
    Json sorted=Json::object();
    for(const auto& key:keys){
      if(key=="__proto__")continue;
      sorted[key]=sortKeysDeep(value[key]);
    }
    return sorted;
  }
  return value;
}
std::string computeChecksumFromSettings(const Json& settings){
  const auto normalized=value::stringify(sortKeysDeep(settings));
  const auto bytes=value::encodeUtf8(normalized);
  std::array<unsigned char,SHA256_DIGEST_LENGTH> digest{};
  SHA256(reinterpret_cast<const unsigned char*>(bytes.data()),bytes.size(),
    digest.data());
  std::ostringstream output;
  output<<"sha256:"<<std::hex<<std::setfill('0');
  for(const auto byte:digest)output<<std::setw(2)<<
    static_cast<unsigned>(byte);
  return output.str();
}
}
