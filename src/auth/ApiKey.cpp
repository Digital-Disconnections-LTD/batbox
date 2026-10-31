#include "auth/ApiKey.hpp"
#include "utils/envUtils.hpp"
#include <stdexcept>

namespace batbox::auth {
namespace {
Json property(const Json& object,const char* key){
  return object.is_object()&&object.contains(key)?object[key]:Json(nullptr);
}
bool approved(const Json& config,const std::string& token){
  const auto responses=property(config,"customApiKeyResponses");
  const auto values=property(responses,"approved");
  if(!values.is_array())return false;
  for(const auto& value:values)if(value==token)return true;
  return false;
}
void requireBindings(const ApiKeyServices& services){
  if(!services.environment||!services.bareMode||
      !services.runningOnHomespace||
      !services.preferThirdPartyAuthentication||!services.flagSettings||
      !services.mergedSettings||!services.globalConfig||
      !services.apiKeyFromFd||!services.apiKeyFromHelperStashed||
      !services.loginManagedKey)
    throw std::invalid_argument("Missing API key selection binding");
}
}
std::string normalizeApiKeyForConfig(const std::string& key){
  return value::slice(key,-20);
}
Json getConfiguredApiKeyHelper(const ApiKeyServices& services){
  if(!services.bareMode||!services.flagSettings||!services.mergedSettings)
    throw std::invalid_argument("Missing API key helper settings binding");
  if(services.bareMode())return property(services.flagSettings(),"apiKeyHelper");
  return property(services.mergedSettings(),"apiKeyHelper");
}
ApiKeyResult getBatboxApiKeyWithSource(const ApiKeyServices& services,
    bool skipRetrievingKeyFromApiKeyHelper){
  requireBindings(services);
  if(services.bareMode()){
    if(value::truthy(services.environment(services.keys.apiKey)))
      return {services.environment(services.keys.apiKey),"BATBOX_API_KEY"};
    if(value::truthy(getConfiguredApiKeyHelper(services)))
      return {skipRetrievingKeyFromApiKeyHelper?Json(nullptr):
        services.apiKeyFromHelperStashed(),"apiKeyHelper"};
    return {};
  }
  const Json apiKeyEnv=services.runningOnHomespace()
    ?Json(nullptr):services.environment(services.keys.apiKey);
  if(services.preferThirdPartyAuthentication()&&value::truthy(apiKeyEnv))
    return {apiKeyEnv,"BATBOX_API_KEY"};
  if(isEnvTruthy(services.environment(services.keys.ci))||
      services.environment(services.keys.nodeEnv)=="test"){
    const auto fromFd=services.apiKeyFromFd();
    if(fromFd&&!fromFd->empty())return {*fromFd,"BATBOX_API_KEY"};
    if(!value::truthy(apiKeyEnv)&&
        !value::truthy(services.environment(services.keys.oauthToken))&&
        !value::truthy(services.environment(services.keys.oauthFd)))
      throw std::runtime_error(
        "BATBOX_API_KEY or BATBOX_OAUTH_TOKEN env var is required");
    if(value::truthy(apiKeyEnv))return {apiKeyEnv,"BATBOX_API_KEY"};
    return {};
  }
  if(value::truthy(apiKeyEnv)&&
      approved(services.globalConfig(),normalizeApiKeyForConfig(
        apiKeyEnv.get<std::string>())))
    return {apiKeyEnv,"BATBOX_API_KEY"};
  const auto fromFd=services.apiKeyFromFd();
  if(fromFd&&!fromFd->empty())return {*fromFd,"BATBOX_API_KEY"};
  if(value::truthy(getConfiguredApiKeyHelper(services)))
    return {skipRetrievingKeyFromApiKeyHelper?Json(nullptr):
      services.apiKeyFromHelperStashed(),"apiKeyHelper"};
  if(const auto login=services.loginManagedKey())return *login;
  return {};
}
Json getBatboxApiKey(const ApiKeyServices& services){
  return getBatboxApiKeyWithSource(services).key;
}
bool hasBatboxApiKeyAuth(const ApiKeyServices& services){
  const auto result=getBatboxApiKeyWithSource(services,true);
  return !result.key.is_null()&&result.source!="none";
}
}
