#include "auth/save/SaveTokens.hpp"
#include "auth/OAuthScopes.hpp"
#include <stdexcept>
#include <vector>

namespace batbox::auth::save {
namespace {
Json property(const Json& value,const std::string& key){
  return value.is_object()&&value.contains(key)?value[key]:Json(nullptr);
}
Json fallback(const Json& preferred,const Json& existing){
  return !preferred.is_null()?preferred:
    !existing.is_null()?existing:Json(nullptr);
}
std::optional<std::vector<std::string>> scopes(const Json& tokens){
  const auto value=property(tokens,"scopes");
  if(!value.is_array())return std::nullopt;
  std::vector<std::string> output;
  for(const auto& scope:value){
    if(scope.is_string())output.push_back(scope.get<std::string>());
  }
  return output;
}
void require(const std::function<void()>& binding,const char* name){
  if(!binding)throw std::invalid_argument(std::string("Missing ")+name+
    " binding");
}
}
Json saveOAuthTokensIfNeeded(const Json& tokens,const Services& services){
  if(!shouldUseBatboxAuth(scopes(tokens),services.inferenceScope))
    return Json{{"success",true}};
  if(!value::truthy(property(tokens,"refreshToken"))||
      !value::truthy(property(tokens,"expiresAt")))
    return Json{{"success",true}};
  if(!services.getSecureStorage)
    throw std::invalid_argument("Missing secure storage binding");
  const auto storage=services.getSecureStorage();
  try{
    if(!storage.read||!storage.update)
      throw std::invalid_argument("Missing secure storage operation");
    auto data=storage.read().value_or(Json::object());
    if(!value::truthy(data))data=Json::object();
    if(!data.is_object())
      throw std::runtime_error("Secure storage returned non-object data");
    const auto existing=property(data,services.storageKey);
    Json oauth=Json::object();
    for(const char* key:{"accessToken","refreshToken","expiresAt",
        "scopes"}){
      if(tokens.is_object()&&tokens.contains(key))oauth[key]=tokens[key];
    }
    oauth["subscriptionType"]=fallback(
      property(tokens,"subscriptionType"),
      property(existing,"subscriptionType"));
    oauth["rateLimitTier"]=fallback(
      property(tokens,"rateLimitTier"),
      property(existing,"rateLimitTier"));
    data[services.storageKey]=oauth;
    const auto status=storage.update(data);
    require(services.clearTokenStash,"OAuth memo cache");
    services.clearTokenStash();
    require(services.clearBetasStashes,"beta cache");
    services.clearBetasStashes();
    require(services.clearGadgetSchemaStash,"tool schema cache");
    services.clearGadgetSchemaStash();
    return status;
  }catch(...){
    if(!services.logError)
      throw std::invalid_argument("Missing local error logger binding");
    services.logError(std::current_exception());
    return Json{{"success",false},
      {"warning","Failed to save OAuth tokens"}};
  }
}
}
