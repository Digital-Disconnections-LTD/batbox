#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::client_factory::oauth_config {
struct Config {
  std::string baseApiUrl,consoleAuthorizeUrl,appAuthorizeUrl,
    appOrigin,tokenUrl,apiKeyUrl,rolesUrl,consoleSuccessUrl,
    appSuccessUrl,manualRedirectUrl,clientId,oauthFileSuffix,
    mcpProxyUrl,mcpProxyPath;
};
struct Routes {
  std::string consoleAuthorize,appAuthorize,token,apiKey,roles,
    consoleSuccess,appSuccess,manualRedirect;
};
struct LocalSpec {
  std::string defaultApiBase,defaultAppBase,defaultConsoleBase,
    clientId,mcpProxyUrl,mcpProxyPath;
  Routes routes;
};
struct Data {
  Config production;
  std::optional<Config> staging;
  LocalSpec local;
  Routes customRoutes;
  std::vector<std::string> allowedCustomBases;
};
struct EnvironmentKeys {
  std::string userType="USER_TYPE";
  std::string internalUserType="batbox";
  std::string useLocal="USE_LOCAL_OAUTH";
  std::string useStaging="USE_STAGING_OAUTH";
  std::string localApiBase="BATBOX_LOCAL_OAUTH_API_BASE";
  std::string localAppBase="BATBOX_LOCAL_OAUTH_APPS_BASE";
  std::string localConsoleBase="BATBOX_LOCAL_OAUTH_CONSOLE_BASE";
  std::string customBase="BATBOX_CUSTOM_OAUTH_URL";
  std::string clientId="BATBOX_OAUTH_CLIENT_ID";
};
struct Services {
  std::function<Json(const std::string&)> environment;
  Data data;
  EnvironmentKeys keys;
};
enum class Kind {Production,Staging,Local};
Kind getOauthConfigType(const Services& services);
std::string fileSuffixForOauthConfig(const Services& services);
Config getLocalOauthConfig(const Services& services);
Config getOauthConfig(const Services& services);
}
