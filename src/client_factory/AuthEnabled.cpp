#include "client_factory/AuthEnabled.hpp"
#include "utils/envUtils.hpp"
#include <stdexcept>

namespace batbox::client_factory {
namespace {
Json env(const AuthEnabledServices& services,const std::string& key){
  if(!services.environment)
    throw std::invalid_argument("Missing auth-enabled environment binding");
  return services.environment(key);
}
}
bool isManagedAuthContext(const AuthEnabledServices& services){
  return isEnvTruthy(env(services,services.keys.remote))||
    env(services,services.keys.entrypoint)==
      services.keys.desktopEntrypoint;
}
bool isBatboxAuthEnabled(const AuthEnabledServices& services){
  if(!services.bareMode||!services.mergedSettings||
      !services.apiKeyWithSource)
    throw std::invalid_argument("Missing auth-enabled source binding");
  if(services.bareMode())return false;
  if(value::truthy(env(services,services.keys.unixSocket)))
    return value::truthy(env(services,services.keys.oauthToken));
  const bool thirdParty=
    isEnvTruthy(env(services,services.keys.useBedrock))||
    isEnvTruthy(env(services,services.keys.useVertex))||
    isEnvTruthy(env(services,services.keys.useFoundry));
  const auto settings=services.mergedSettings();
  const auto helper=value::property(settings,"apiKeyHelper");
  const bool hasExternalToken=
    value::truthy(env(services,services.keys.authToken))||
    value::truthy(helper)||
    value::truthy(env(services,services.keys.apiKeyFd));
  const auto keySource=services.apiKeyWithSource(true).source;
  const bool hasExternalKey=
    keySource==services.keys.externalApiKeySource||
    keySource=="apiKeyHelper";
  const bool disableAuth=thirdParty||
    (hasExternalToken&&!isManagedAuthContext(services))||
    (hasExternalKey&&!isManagedAuthContext(services));
  return !disableAuth;
}
}
