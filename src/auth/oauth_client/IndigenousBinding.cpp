#include "auth/oauth_client/IndigenousBinding.hpp"
#include "auth/refresh/IndigenousHttp.hpp"
#include <utility>

namespace batbox::auth::oauth_client {
AuthUrlServices makeIndigenousAuthUrlServices(
    client_factory::oauth_config::Services config,
    std::vector<std::string> allScopes,std::string inferenceScope){
  AuthUrlServices result;
  result.getConfig=[config=std::move(config)]{
    return client_factory::oauth_config::getOauthConfig(config);};
  result.allScopes=std::move(allScopes);
  result.inferenceScope=std::move(inferenceScope);
  return result;
}
ExchangeServices makeIndigenousExchangeServices(event_loop::Loop& loop,
    client_factory::oauth_config::Services config){
  ExchangeServices result;
  result.getConfig=[config=std::move(config)]{
    return client_factory::oauth_config::getOauthConfig(config);};
  result.httpPost=[&loop](const std::string& url,const Json& body,
      const Json& headers,double timeoutMs){
    return auth::refresh::indigenousHttpPost(loop,url,body,headers,timeoutMs);};
  return result;
}
}
