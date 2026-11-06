#include "auth/storage/KeychainHelpers.hpp"
#include "runtime/Encoding.hpp"
#include <array>
#include <iomanip>
#include <openssl/sha.h>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace batbox::auth::storage {
KeychainState::KeychainState(KeychainServices services):
    services_(std::move(services)){
  if(!services_.configHome||!services_.configDirOverride||
      !services_.userEnv||!services_.osUsername||!services_.nowMs||
      !services_.oauthFileSuffix)
    throw std::invalid_argument("Missing keychain helper binding");
}
std::string KeychainState::serviceName(const std::string& suffix)const{
  const auto directory=services_.configHome();
  std::string hashSuffix;
  const auto override=services_.configDirOverride();
  if(!override||override->empty()){
    return "Batbox"+services_.oauthFileSuffix()+suffix;
  }
  const auto bytes=value::encodeUtf8(directory);
  std::array<unsigned char,SHA256_DIGEST_LENGTH> digest{};
  SHA256(reinterpret_cast<const unsigned char*>(bytes.data()),
    bytes.size(),digest.data());
  std::ostringstream output;
  output<<'-'<<std::hex<<std::setfill('0');
  for(int index=0;index<4;++index)
    output<<std::setw(2)<<static_cast<int>(digest[index]);
  hashSuffix=output.str();
  return "Batbox"+services_.oauthFileSuffix()+suffix+hashSuffix;
}
std::string KeychainState::username()const{
  try{
    const auto value=services_.userEnv();
    if(value&&!value->empty())return *value;
    return services_.osUsername();
  }catch(...){return "batbox-user";}
}
void KeychainState::clear(){
  data.reset();stashedAt=0;++generation;readInFlight.reset();
}
}
