#include "auth/oauth_client/IndigenousAccountBinding.hpp"
#include "auth/oauth_client/IndigenousAccountHttp.hpp"
#include "auth/refresh/IndigenousHttp.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::auth::oauth_client {
AccountServices makeIndigenousAccountServices(event_loop::Loop& loop,
    client_factory::oauth_config::Services config,
    std::function<Json()> getGlobalConfig,
    std::function<void(const std::function<Json(const Json&)>&)>
      saveGlobalConfig,
    std::shared_ptr<auth::save::ApiKeySaver> saver){
  if(!getGlobalConfig||!saveGlobalConfig||!saver)
    throw std::invalid_argument("Missing OAuth account persistence binding");
  AccountServices result;
  result.getConfig=[config=std::move(config)]{
    return client_factory::oauth_config::getOauthConfig(config);};
  result.httpGet=[&loop](const std::string& url,const Json& headers){
    return auth::refresh::indigenousHttpGet(loop,url,headers,0);};
  result.httpPost=[&loop](const std::string& url,const Json& body,
      const Json& headers){
    if(!body.is_null())
      throw std::invalid_argument("OAuth API-key POST body must be null");
    return indigenousEmptyPost(loop,url,headers);};
  result.getGlobalConfig=std::move(getGlobalConfig);
  result.saveGlobalConfig=std::move(saveGlobalConfig);
  result.saveApiKey=[saver=std::move(saver)](const std::string& key){
    return saver->save(key);};
  return result;
}
}
