#include "paste_store/PasteStore.hpp"
#include "runtime/Encoding.hpp"
#include <openssl/evp.h>
#include <stdexcept>

namespace batbox::paste_store {
std::string getPasteStoreDir(const Services& services){
  if(!services.getConfigHomeDir||!services.join)
    throw std::invalid_argument("Paste store path imports required");
  return services.join(services.getConfigHomeDir(),"paste-cache");
}
std::string getPastePath(const Services& services,const std::string& hash){
  if(!services.join)throw std::invalid_argument("Paste store join import required");
  return services.join(getPasteStoreDir(services),hash+".txt");
}
std::string hashPastedText(const std::string& content){
  const auto bytes=value::encodeUtf8(content);
  unsigned char digest[EVP_MAX_MD_SIZE];unsigned length=0;
  if(EVP_Digest(bytes.data(),bytes.size(),digest,&length,EVP_sha256(),nullptr)!=1||
      length<8)throw std::runtime_error("SHA-256 failed");
  static constexpr char digits[]="0123456789abcdef";
  std::string result;result.reserve(16);
  for(int index=0;index<8;++index){
    result.push_back(digits[digest[index]>>4]);
    result.push_back(digits[digest[index]&15]);
  }
  return result;
}
}
