#include "auth/storage/MacKeychain.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::auth::storage {
KeychainLockDetector::KeychainLockDetector(
    std::function<std::string()> platform,
    std::function<SecurityResult()> showKeychainInfo):
  platform_(std::move(platform)),
  showKeychainInfo_(std::move(showKeychainInfo)){
  if(!platform_||!showKeychainInfo_)
    throw std::invalid_argument("Missing keychain lock detector binding");
}
bool KeychainLockDetector::isLocked(){
  if(stashed_)return *stashed_;
  if(platform_()!="darwin"){
    stashed_=false;return false;
  }
  try{stashed_=showKeychainInfo_().exitCode==36;}
  catch(...){stashed_=false;}
  return *stashed_;
}
}
