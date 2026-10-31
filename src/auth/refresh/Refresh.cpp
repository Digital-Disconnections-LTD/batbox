#include "auth/refresh/Refresh.hpp"
#include "auth/OAuthScopes.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace batbox::auth::refresh {
namespace {
Json property(const Json& object,const char* key){
  return object.is_object()&&object.contains(key)?object[key]:Json(nullptr);
}
bool has(const Json& object,const char* key){
  return object.is_object()&&object.contains(key);
}
std::string join(const std::vector<std::string>& values){
  std::string result;
  for(const auto& value:values){
    if(!result.empty())result.push_back(' ');
    result+=value;
  }
  return result;
}
void saveProfileFields(const Json& profile,const Json& config,
    const Services& services){
  if(profile.is_null()||!value::truthy(property(config,"oauthAccount")))return;
  Json updates=Json::object();
  if(has(profile,"displayName"))updates["displayName"]=profile["displayName"];
  if(property(profile,"hasExtraUsageEnabled").is_boolean())
    updates["hasExtraUsageEnabled"]=profile["hasExtraUsageEnabled"];
  if(!property(profile,"billingType").is_null())
    updates["billingType"]=profile["billingType"];
  if(has(profile,"accountCreatedAt"))
    updates["accountCreatedAt"]=profile["accountCreatedAt"];
  if(has(profile,"subscriptionCreatedAt"))
    updates["subscriptionCreatedAt"]=profile["subscriptionCreatedAt"];
  if(updates.empty())return;
  services.saveGlobalConfig([updates](const Json& current){
    Json next=current;
    if(has(current,"oauthAccount")){
      if(value::truthy(current["oauthAccount"])){
        Json account=current["oauthAccount"];
        for(auto it=updates.begin();it!=updates.end();++it)
          account[it.key()]=it.value();
        next["oauthAccount"]=account;
      }else next["oauthAccount"]=current["oauthAccount"];
    }
    return next;
  });
}
Json tokenAccount(const Json& data){
  const auto account=property(data,"account");
  if(!value::truthy(account))return nullptr;
  Json result=Json::object();
  if(has(account,"uuid"))result["uuid"]=account["uuid"];
  if(has(account,"email_address"))
    result["emailAddress"]=account["email_address"];
  const auto organization=property(data,"organization");
  if(has(organization,"uuid"))
    result["organizationUuid"]=organization["uuid"];
  return result;
}
}
event_loop::Deferred<Json> refreshOAuthToken(event_loop::Loop& loop,
    const std::string& refreshToken,
    const std::optional<std::vector<std::string>>& requestedScopes,
    const Services& services){
  if(!services.tokenUrl||!services.clientId||!services.httpPost||
      !services.nowMs||!services.globalConfig||!services.existingTokens||
      !services.fetchProfileInfo||!services.saveGlobalConfig)
    throw std::invalid_argument("Missing OAuth refresh binding");
  event_loop::Deferred<Json> output(loop);
  try{
    const auto clientId=services.clientId();
    const auto& scopes=requestedScopes&&!requestedScopes->empty()
      ?*requestedScopes:services.defaultScopes;
    const Json body={{"grant_type","refresh_token"},
      {"refresh_token",refreshToken},
      {"client_id",clientId},{"scope",join(scopes)}};
    const auto url=services.tokenUrl();
    services.httpPost(url,body,Json{{"Content-Type","application/json"}},
        15000).then([output,services,&loop,refreshToken](
          const HttpResponse* response,const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{
        if(response->status!=200)
          throw std::runtime_error("Token refresh failed: "+
            response->statusText);
        const auto data=response->data;
        const auto accessToken=property(data,"access_token");
        const auto newRefreshToken=has(data,"refresh_token")
          ?data["refresh_token"]:Json(refreshToken);
        const auto seconds=has(data,"expires_in")
          ?value::numberOr(data["expires_in"],
            std::numeric_limits<double>::quiet_NaN())
          :std::numeric_limits<double>::quiet_NaN();
        const auto expires=services.nowMs()+seconds*1000;
        const Json expiresAt=std::isfinite(expires)?Json(expires):Json(nullptr);
        std::optional<std::string> scope;
        if(has(data,"scope")&&data["scope"].is_string())
          scope=data["scope"].get<std::string>();
        const auto parsedScopes=parseScopes(scope);
        const auto config=services.globalConfig();
        const auto existing=services.existingTokens();
        const auto configAccount=property(config,"oauthAccount");
        const auto existingValue=existing?*existing:Json(nullptr);
        const bool haveProfileAlready=
          has(configAccount,"billingType")&&
          has(configAccount,"accountCreatedAt")&&
          has(configAccount,"subscriptionCreatedAt")&&
          !property(existingValue,"subscriptionType").is_null()&&
          !property(existingValue,"rateLimitTier").is_null();
        const auto finish=[output,services,data,config,existingValue,
            accessToken,newRefreshToken,expiresAt,parsedScopes](
            const Json& profile){
          try{
            saveProfileFields(profile,config,services);
            Json result={{"accessToken",accessToken},
              {"refreshToken",newRefreshToken},
              {"expiresAt",expiresAt},
              {"scopes",parsedScopes},
              {"subscriptionType",nullptr},
              {"rateLimitTier",nullptr}};
            const auto subscription=property(profile,"subscriptionType");
            const auto priorSubscription=property(existingValue,
              "subscriptionType");
            if(!subscription.is_null())
              result["subscriptionType"]=subscription;
            else if(!priorSubscription.is_null())
              result["subscriptionType"]=priorSubscription;
            const auto tier=property(profile,"rateLimitTier");
            const auto priorTier=property(existingValue,"rateLimitTier");
            if(!tier.is_null())result["rateLimitTier"]=tier;
            else if(!priorTier.is_null())result["rateLimitTier"]=priorTier;
            if(has(profile,"rawProfile"))
              result["profile"]=profile["rawProfile"];
            if(value::truthy(property(data,"account")))
              result["tokenAccount"]=tokenAccount(data);
            output.resolve(result);
          }catch(...){output.reject(std::current_exception());}
        };
        if(haveProfileAlready){finish(nullptr);return;}
        services.fetchProfileInfo(accessToken.get<std::string>()).then(
          [output,finish](const Json* profile,
              const std::exception_ptr& profileError){
            if(profileError)output.reject(profileError);
            else finish(*profile);
          });
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
}
