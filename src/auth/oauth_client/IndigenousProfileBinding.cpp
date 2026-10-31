#include "auth/oauth_client/IndigenousProfileBinding.hpp"
#include "auth/oauth_client/ProfileScope.hpp"
#include "auth/refresh/IndigenousHttp.hpp"
#include "auth/refresh/ProfileHttp.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::auth::oauth_client {
ProfileAccountServices makeIndigenousProfileAccountServices(
    event_loop::Loop& loop,
    client_factory::oauth_config::Services config,
    std::shared_ptr<auth::refresh::CheckController> checker,
    std::function<std::optional<Json>()> getOAuthTokens,
    std::function<Json()> getGlobalConfig,
    std::function<void(const std::function<Json(const Json&)>&)>
      saveGlobalConfig,
    std::function<Json(const std::string&)> environment,
    std::function<bool()> isSubscriber,
    std::function<void(const std::string&)> logForDebugging,
    std::function<void(const std::string&)> logProfileError,
    std::string profileScope){
  if(!checker||!getOAuthTokens||!getGlobalConfig||!saveGlobalConfig||
      !environment||!isSubscriber||!logForDebugging||
      !logProfileError||profileScope.empty())
    throw std::invalid_argument("Missing native OAuth profile binding");
  ProfileAccountServices result;
  result.getGlobalConfig=std::move(getGlobalConfig);
  result.saveGlobalConfig=std::move(saveGlobalConfig);
  result.environment=std::move(environment);
  result.getOAuthTokens=std::move(getOAuthTokens);
  result.isSubscriber=std::move(isSubscriber);
  result.logForDebugging=std::move(logForDebugging);
  result.hasProfileScope=[tokens=result.getOAuthTokens,
      scope=std::move(profileScope)]{
    return oauth_client::hasProfileScope(tokens,scope);};
  result.checkAndRefresh=[&loop,checker]{
    event_loop::Deferred<event_loop::Unit> output(loop);
    try{checker->check().then([output](const bool*,
        const std::exception_ptr& error){
      if(error)output.reject(error);else output.resolve({});
    });}catch(...){output.reject(std::current_exception());}
    return output;
  };
  auth::refresh::ProfileHttpServices profile;
  profile.baseApiUrl=[config=std::move(config)]{
    return client_factory::oauth_config::getOauthConfig(config).baseApiUrl;};
  profile.httpGet=[&loop](const std::string& url,const Json& headers,
      double timeout){
    return auth::refresh::indigenousHttpGet(loop,url,headers,timeout);};
  profile.logError=std::move(logProfileError);
  result.getRawProfile=[&loop,profile=std::move(profile)](
      const std::string& token){
    return auth::refresh::getRawOAuthProfile(loop,token,profile);};
  return result;
}
}
