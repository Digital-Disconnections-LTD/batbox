#include "settings/remote/Auth.hpp"
#include <stdexcept>

namespace batbox::settings::remote {
std::string getRemoteManagedSettingsEndpoint(const std::string& baseApiUrl,
    const std::string& settingsPath){return baseApiUrl+settingsPath;}
AuthHeaders getRemoteSettingsAuthHeaders(const AuthServices& services){
  if(!services.apiKeyWithoutHelper||!services.oauthTokens)
    throw std::invalid_argument("Missing remote auth binding");
  try{
    const auto key=services.apiKeyWithoutHelper();
    if(key&&!key->empty())
      return {Json{{services.apiKeyHeader,*key}},std::nullopt};
  }catch(...){}
  const auto tokens=services.oauthTokens();
  if(tokens.is_object()&&tokens.contains("accessToken")&&
      value::truthy(tokens["accessToken"])){
    const auto token=tokens["accessToken"].is_string()
      ?tokens["accessToken"].get<std::string>()
      :value::stringify(tokens["accessToken"]);
    return {Json{{"Authorization","Bearer "+token},
      {services.oauthBetaHeader,services.oauthBetaValue}},std::nullopt};
  }
  return {Json::object(),"No authentication available"};
}
}
