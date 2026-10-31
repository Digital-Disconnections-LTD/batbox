#include "auth/oauth_client/ProfileAccount.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::auth::oauth_client {
namespace {
Json property(const Json& object,const char* key){
  return object.is_object()&&object.contains(key)?object[key]:Json(nullptr);
}
bool has(const Json& object,const char* key){
  return object.is_object()&&object.contains(key);
}
std::optional<std::string> textIfPresent(const Json& value){
  return value.is_string()?std::optional(value.get<std::string>()):
    std::nullopt;
}
Json tokenAccess(const ProfileAccountServices& services){
  if(!services.getOAuthTokens)
    throw std::invalid_argument("Missing OAuth token binding");
  const auto tokens=services.getOAuthTokens();
  return tokens?property(*tokens,"accessToken"):Json(nullptr);
}
Json accountInfoFrom(const Json& fields){
  Json account=Json::object();
  for(const char* key:{"accountUuid","emailAddress","organizationUuid",
      "hasExtraUsageEnabled","billingType","accountCreatedAt",
      "subscriptionCreatedAt"})
    if(has(fields,key))account[key]=fields[key];
  if(value::truthy(property(fields,"displayName")))
    account["displayName"]=fields["displayName"];
  return account;
}
bool sameAccountFields(const Json& old,const Json& current){
  for(const char* key:{"accountUuid","emailAddress","organizationUuid",
      "displayName","hasExtraUsageEnabled","billingType",
      "accountCreatedAt","subscriptionCreatedAt"}){
    if(has(old,key)!=has(current,key))return false;
    if(has(old,key)&&old[key]!=current[key])return false;
  }
  return true;
}
bool completeStashedProfile(const Json& config){
  const auto account=property(config,"oauthAccount");
  return value::truthy(account)&&has(account,"billingType")&&
    has(account,"accountCreatedAt")&&
    has(account,"subscriptionCreatedAt");
}
Json fieldIfPresent(const Json& object,const char* key){
  return has(object,key)?object[key]:Json(nullptr);
}
}
void storeOAuthAccountInfo(const Json& fields,
    const ProfileAccountServices& services){
  if(!services.saveGlobalConfig)
    throw std::invalid_argument("Missing OAuth account save binding");
  const Json info=accountInfoFrom(fields);
  services.saveGlobalConfig([info](const Json& current){
    if(sameAccountFields(property(current,"oauthAccount"),info))
      return current;
    Json updated=current.is_object()?current:Json::object();
    updated["oauthAccount"]=info;
    return updated;
  });
}
event_loop::Deferred<std::optional<std::string>> getOrganizationUUID(
    event_loop::Loop& loop,const ProfileAccountServices& services){
  event_loop::Deferred<std::optional<std::string>> output(loop);
  try{
    if(!services.getGlobalConfig||!services.hasProfileScope||
        !services.getRawProfile)
      throw std::invalid_argument("Missing OAuth organization binding");
    const auto config=services.getGlobalConfig();
    const auto uuid=property(property(config,"oauthAccount"),
      "organizationUuid");
    if(value::truthy(uuid)){
      output.resolve(textIfPresent(uuid));return output;
    }
    const auto access=tokenAccess(services);
    if(access.is_null()||!services.hasProfileScope()){
      output.resolve(std::nullopt);return output;
    }
    if(!access.is_string())
      throw std::invalid_argument("OAuth access token must be a string");
    services.getRawProfile(access.get<std::string>()).then([output](
        const std::optional<Json>* profile,
        const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      const auto organization=profile&&*profile?
        property(**profile,"organization"):Json(nullptr);
      const auto uuid=property(organization,"uuid");
      output.resolve(value::truthy(uuid)?textIfPresent(uuid):std::nullopt);
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<bool> populateOAuthAccountInfoIfNeeded(
    event_loop::Loop& loop,const ProfileAccountServices& services){
  event_loop::Deferred<bool> output(loop);
  try{
    if(!services.environment||!services.getGlobalConfig||
        !services.checkAndRefresh||!services.isSubscriber||
        !services.hasProfileScope||!services.getRawProfile)
      throw std::invalid_argument("Missing OAuth profile population binding");
    const auto accountUuid=services.environment(services.accountUuidEnv);
    const auto email=services.environment(services.userEmailEnv);
    const auto organizationUuid=services.environment(
      services.organizationUuidEnv);
    const bool hasEnvVars=value::truthy(accountUuid)&&value::truthy(email)&&
      value::truthy(organizationUuid);
    if(hasEnvVars){
      if(!value::truthy(property(services.getGlobalConfig(),"oauthAccount")))
        storeOAuthAccountInfo(Json{{"accountUuid",accountUuid},
          {"emailAddress",email},{"organizationUuid",organizationUuid}},
          services);
    }
    services.checkAndRefresh().then([output,services,hasEnvVars](
        const event_loop::Unit*,const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{
        const auto config=services.getGlobalConfig();
        if(completeStashedProfile(config)||!services.isSubscriber()||
            !services.hasProfileScope()){
          output.resolve(false);return;
        }
        const auto access=tokenAccess(services);
        if(!value::truthy(access)){
          output.resolve(false);return;
        }
        if(!access.is_string())
          throw std::invalid_argument("OAuth access token must be a string");
        services.getRawProfile(access.get<std::string>()).then(
          [output,services,hasEnvVars](const std::optional<Json>* profile,
              const std::exception_ptr& profileError){
          if(profileError){output.reject(profileError);return;}
          try{
            if(!profile||!*profile||!value::truthy(**profile)){
              output.resolve(false);return;
            }
            const auto& raw=**profile;
            const auto account=property(raw,"account");
            const auto org=property(raw,"organization");
            if(!account.is_object()||!org.is_object())
              throw std::invalid_argument("OAuth profile account is incomplete");
            if(hasEnvVars&&services.logForDebugging)
              services.logForDebugging(
                "OAuth profile fetch succeeded, overriding env account info");
            Json fields={{"accountUuid",fieldIfPresent(account,"uuid")},
              {"emailAddress",fieldIfPresent(account,"email")},
              {"organizationUuid",fieldIfPresent(org,"uuid")},
              {"hasExtraUsageEnabled",has(org,"has_extra_usage_enabled")&&
                !org["has_extra_usage_enabled"].is_null()?
                org["has_extra_usage_enabled"]:Json(false)}};
            if(value::truthy(property(account,"display_name")))
              fields["displayName"]=account["display_name"];
            if(has(org,"billing_type")&&!org["billing_type"].is_null())
              fields["billingType"]=org["billing_type"];
            if(has(account,"created_at"))
              fields["accountCreatedAt"]=account["created_at"];
            if(has(org,"subscription_created_at")&&
                !org["subscription_created_at"].is_null())
              fields["subscriptionCreatedAt"]=
                org["subscription_created_at"];
            storeOAuthAccountInfo(fields,services);
            output.resolve(true);
          }catch(...){output.reject(std::current_exception());}
        });
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
}
