#include "auth/Provider.hpp"
#include "runtime/Url.hpp"
#include "utils/envUtils.hpp"
#include <stdexcept>

namespace batbox::auth {
ApiProvider getAPIProvider(const ProviderServices& services){
  if(!services.environment)
    throw std::invalid_argument("Missing provider environment binding");
  if(isEnvTruthy(services.environment(services.keys.useBedrock)))
    return ApiProvider::Bedrock;
  if(isEnvTruthy(services.environment(services.keys.useVertex)))
    return ApiProvider::Vertex;
  if(isEnvTruthy(services.environment(services.keys.useFoundry)))
    return ApiProvider::Foundry;
  return ApiProvider::FirstParty;
}
bool isFirstPartyBaseUrl(const ProviderServices& services){
  if(!services.environment||!services.internalUser||
      services.primaryHost.empty())
    throw std::invalid_argument("Missing provider base URL binding");
  const auto value=services.environment(services.keys.baseUrl);
  if(!value::truthy(value))return true;
  try{
    if(!value.is_string())return false;
    const auto parsed=runtime::parseUrl(value.get<std::string>());
    if(!parsed)return false;
    const auto host=(*parsed)["host"].get<std::string>();
    const auto internal=services.internalUser();
    if(host==services.primaryHost)return true;
    return internal&&!services.stagingHost.empty()&&
      host==services.stagingHost;
  }catch(...){return false;}
}
std::string providerName(ApiProvider provider){
  switch(provider){
    case ApiProvider::FirstParty:return "firstParty";
    case ApiProvider::Bedrock:return "bedrock";
    case ApiProvider::Vertex:return "vertex";
    case ApiProvider::Foundry:return "foundry";
  }
  throw std::invalid_argument("Unknown API provider");
}
}
