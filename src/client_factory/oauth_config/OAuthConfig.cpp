#include "client_factory/oauth_config/OAuthConfig.hpp"
#include "utils/envUtils.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::client_factory::oauth_config {
namespace {
Json env(const Services& services,const std::string& name){
  if(!services.environment)
    throw std::invalid_argument("Missing OAuth config environment binding");
  return services.environment(name);
}
std::optional<std::string> optionalText(const Json& value){
  if(value.is_null())return std::nullopt;
  if(!value.is_string())throw std::invalid_argument(
    "OAuth configuration environment must be a string");
  return value.get<std::string>();
}
std::string removeOneSlash(std::string value){
  if(!value.empty()&&value.back()=='/')value.pop_back();
  return value;
}
std::string localBase(const Services& services,const std::string& key,
    const std::string& fallback){
  const auto value=optionalText(env(services,key));
  return value?removeOneSlash(*value):fallback;
}
}
Kind getOauthConfigType(const Services& services){
  if(env(services,services.keys.userType)==
      services.keys.internalUserType){
    if(isEnvTruthy(env(services,services.keys.useLocal)))
      return Kind::Local;
    if(isEnvTruthy(env(services,services.keys.useStaging)))
      return Kind::Staging;
  }
  return Kind::Production;
}
std::string fileSuffixForOauthConfig(const Services& services){
  if(value::truthy(env(services,services.keys.customBase)))
    return "-custom-oauth";
  switch(getOauthConfigType(services)){
    case Kind::Local:return "-local-oauth";
    case Kind::Staging:return "-staging-oauth";
    case Kind::Production:return {};
  }
  throw std::logic_error("Unknown OAuth config type");
}
Config getLocalOauthConfig(const Services& services){
  const auto& spec=services.data.local;
  if(spec.defaultApiBase.empty()||spec.defaultAppBase.empty()||
      spec.defaultConsoleBase.empty()||spec.clientId.empty()||
      spec.routes.consoleAuthorize.empty()||
      spec.routes.appAuthorize.empty()||spec.routes.token.empty()||
      spec.routes.apiKey.empty()||spec.routes.roles.empty()||
      spec.routes.consoleSuccess.empty()||
      spec.routes.appSuccess.empty()||
      spec.routes.manualRedirect.empty())
    throw std::invalid_argument("Missing local OAuth endpoint data");
  const auto api=localBase(services,services.keys.localApiBase,
    spec.defaultApiBase);
  const auto app=localBase(services,services.keys.localAppBase,
    spec.defaultAppBase);
  const auto console=localBase(services,services.keys.localConsoleBase,
    spec.defaultConsoleBase);
  const auto& route=spec.routes;
  return Config{api,console+route.consoleAuthorize,
    app+route.appAuthorize,app,api+route.token,api+route.apiKey,
    api+route.roles,console+route.consoleSuccess,
    console+route.appSuccess,console+route.manualRedirect,
    spec.clientId,"-local-oauth",spec.mcpProxyUrl,spec.mcpProxyPath};
}
Config getOauthConfig(const Services& services){
  Config config;
  switch(getOauthConfigType(services)){
    case Kind::Local:config=getLocalOauthConfig(services);break;
    case Kind::Staging:
      config=services.data.staging.value_or(services.data.production);break;
    case Kind::Production:config=services.data.production;break;
  }
  const auto custom=optionalText(env(services,services.keys.customBase));
  if(custom&&!custom->empty()){
    const auto base=removeOneSlash(*custom);
    if(std::find(services.data.allowedCustomBases.begin(),
        services.data.allowedCustomBases.end(),base)==
        services.data.allowedCustomBases.end())
      throw std::runtime_error(
        "BATBOX_CUSTOM_OAUTH_URL is not an approved endpoint.");
    const auto& route=services.data.customRoutes;
    if(route.consoleAuthorize.empty()||route.appAuthorize.empty()||
        route.token.empty()||route.apiKey.empty()||route.roles.empty()||
        route.consoleSuccess.empty()||route.appSuccess.empty()||
        route.manualRedirect.empty())
      throw std::invalid_argument("Missing custom OAuth route data");
    config.baseApiUrl=base;
    config.consoleAuthorizeUrl=base+route.consoleAuthorize;
    config.appAuthorizeUrl=base+route.appAuthorize;
    config.appOrigin=base;
    config.tokenUrl=base+route.token;
    config.apiKeyUrl=base+route.apiKey;
    config.rolesUrl=base+route.roles;
    config.consoleSuccessUrl=base+route.consoleSuccess;
    config.appSuccessUrl=base+route.appSuccess;
    config.manualRedirectUrl=base+route.manualRedirect;
    config.oauthFileSuffix="-custom-oauth";
  }
  const auto clientId=optionalText(env(services,services.keys.clientId));
  if(clientId&&!clientId->empty())config.clientId=*clientId;
  return config;
}
}
