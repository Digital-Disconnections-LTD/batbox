#include "auth/oauth_client/AccountActions.hpp"
#include <stdexcept>

namespace batbox::auth::oauth_client {
namespace {
Json property(const Json& object,const char* key){
  return object.is_object()&&object.contains(key)?object[key]:Json(nullptr);
}
Json applyRoles(const Json& current,const Json& data){
  Json next=current;
  if(!next.is_object())return next;
  const auto account=property(current,"oauthAccount");
  if(!value::truthy(account))return next;
  Json updated=account;
  if(!updated.is_object())return next;
  for(const auto& [source,target]:{
      std::pair{"organization_role","organizationRole"},
      std::pair{"workspace_role","workspaceRole"},
      std::pair{"organization_name","organizationName"}}){
    if(data.is_object()&&data.contains(source))
      updated[target]=data[source];
    else updated.erase(target);
  }
  next["oauthAccount"]=updated;
  return next;
}
}
event_loop::Deferred<event_loop::Unit> fetchAndStoreUserRoles(
    event_loop::Loop& loop,const std::string& accessToken,
    const AccountServices& services){
  event_loop::Deferred<event_loop::Unit> output(loop);
  try{
    if(!services.getConfig||!services.httpGet||
        !services.getGlobalConfig||!services.saveGlobalConfig)
      throw std::invalid_argument("Missing OAuth roles binding");
    const auto url=services.getConfig().rolesUrl;
    services.httpGet(url,Json{{"Authorization","Bearer "+accessToken}})
      .then([output,services](const auth::refresh::HttpResponse* response,
          const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{
        if(response->status!=200)
          throw std::runtime_error("Failed to fetch user roles: "+
            response->statusText);
        const auto data=response->data;
        const auto config=services.getGlobalConfig();
        if(!value::truthy(property(config,"oauthAccount")))
          throw std::runtime_error(
            "OAuth account information not found in config");
        services.saveGlobalConfig([data](const Json& current){
          return applyRoles(current,data);});
        output.resolve({});
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<std::optional<std::string>> createAndStoreApiKey(
    event_loop::Loop& loop,const std::string& accessToken,
    const AccountServices& services){
  event_loop::Deferred<std::optional<std::string>> output(loop);
  try{
    if(!services.getConfig||!services.httpPost||!services.saveApiKey)
      throw std::invalid_argument("Missing OAuth API-key binding");
    const auto url=services.getConfig().apiKeyUrl;
    services.httpPost(url,Json(nullptr),
      Json{{"Authorization","Bearer "+accessToken}}).then(
        [output,services](const auth::refresh::HttpResponse* response,
            const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{
        const auto key=property(response->data,"raw_key");
        if(!value::truthy(key)){output.resolve(std::nullopt);return;}
        if(!key.is_string())
          throw std::invalid_argument("OAuth API key must be a string");
        const auto text=key.get<std::string>();
        services.saveApiKey(text).then([output,text](
            const event_loop::Unit*,const std::exception_ptr& saveError){
          if(saveError)output.reject(saveError);else output.resolve(text);
        });
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
}
