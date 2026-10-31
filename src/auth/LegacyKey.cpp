#include "auth/LegacyKey.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::auth {
LegacyKeyReader::LegacyKeyReader(LegacyKeyServices services):
    services_(std::move(services)){
  if(!services_.bareMode||!services_.platform||
      !services_.legacyKeychainPrefetch||!services_.keychainServiceName||
      !services_.executeSync||!services_.logError||!services_.globalConfig)
    throw std::invalid_argument("Missing login-managed key binding");
}
std::optional<ApiKeyResult> LegacyKeyReader::getLoginManagedKey(){
  if(stashed_)return *stashed_;
  if(services_.bareMode()){
    stashed_=std::optional<ApiKeyResult>{};return std::nullopt;
  }
  if(services_.platform()=="darwin"){
    const auto prefetch=services_.legacyKeychainPrefetch();
    if(prefetch&&value::truthy(*prefetch)){
      if(prefetch->is_object()&&prefetch->contains("stdout")&&
          value::truthy((*prefetch)["stdout"])){
        stashed_=ApiKeyResult{(*prefetch)["stdout"],"/login managed key"};
        return *stashed_;
      }
    }else{
      const auto service=services_.keychainServiceName();
      try{
        const auto result=services_.executeSync(
          "security find-generic-password -a $USER -w -s \""+service+"\"");
        if(result&&!result->empty()){
          stashed_=ApiKeyResult{*result,"/login managed key"};
          return *stashed_;
        }
      }catch(const std::exception& error){services_.logError(error.what());}
    }
  }
  const auto config=services_.globalConfig();
  if(!config.is_object()||!config.contains("primaryApiKey")||
      !value::truthy(config["primaryApiKey"])){
    stashed_=std::optional<ApiKeyResult>{};return std::nullopt;
  }
  stashed_=ApiKeyResult{config["primaryApiKey"],"/login managed key"};
  return *stashed_;
}
void LegacyKeyReader::clearStash(){stashed_.reset();}
}
