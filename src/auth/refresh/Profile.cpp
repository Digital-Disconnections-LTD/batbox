#include "auth/refresh/Refresh.hpp"
#include <stdexcept>

namespace batbox::auth::refresh {
namespace {
Json property(const Json& object,const char* key){
  return object.is_object()&&object.contains(key)?object[key]:Json(nullptr);
}
bool truthy(const Json& object,const char* key){
  return value::truthy(property(object,key));
}
}
event_loop::Deferred<Json> fetchProfileInfo(event_loop::Loop& loop,
    const std::string& accessToken,const ProfileServices& services){
  if(!services.getRawProfile||services.subscriptionTypes.empty())
    throw std::invalid_argument("Missing OAuth profile binding");
  event_loop::Deferred<Json> output(loop);
  try{
    services.getRawProfile(accessToken).then([output,types=services.subscriptionTypes](
        const std::optional<Json>* profile,const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{
        const Json value=*profile?**profile:Json(nullptr);
        const Json organization=property(value,"organization");
        const Json account=property(value,"account");
        Json subscription=nullptr;
        const Json orgType=property(organization,"organization_type");
        if(orgType.is_string()){
          const auto found=types.find(orgType.get<std::string>());
          if(found!=types.end())subscription=found->second;
        }
        Json result={{"subscriptionType",subscription},
          {"rateLimitTier",property(organization,"rate_limit_tier")},
          {"hasExtraUsageEnabled",property(organization,
            "has_extra_usage_enabled")},
          {"billingType",property(organization,"billing_type")}};
        if(truthy(account,"display_name"))
          result["displayName"]=account["display_name"];
        if(truthy(account,"created_at"))
          result["accountCreatedAt"]=account["created_at"];
        if(truthy(organization,"subscription_created_at"))
          result["subscriptionCreatedAt"]=
            organization["subscription_created_at"];
        if(*profile)result["rawProfile"]=**profile;
        output.resolve(result);
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
}
