#include "auth/oauth_client/TokenExchange.hpp"
#include <stdexcept>

namespace batbox::auth::oauth_client {
event_loop::Deferred<Json> exchangeCodeForTokens(event_loop::Loop& loop,
    const ExchangeArgs& args,const ExchangeServices& services){
  event_loop::Deferred<Json> output(loop);
  try{
    if(!services.getConfig||!services.httpPost)
      throw std::invalid_argument("Missing OAuth code-exchange binding");
    Json body={{"grant_type","authorization_code"},
      {"code",args.authorizationCode},
      {"redirect_uri",args.useManualRedirect?
        services.getConfig().manualRedirectUrl:
        "http://localhost:"+std::to_string(args.port)+"/callback"},
      {"client_id",services.getConfig().clientId},
      {"code_verifier",args.codeVerifier},{"state",args.state}};
    if(args.expiresIn)body["expires_in"]=*args.expiresIn;
    const auto tokenUrl=services.getConfig().tokenUrl;
    services.httpPost(tokenUrl,body,
      Json{{"Content-Type","application/json"}},15000).then(
        [output](const auth::refresh::HttpResponse* response,
            const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{
        if(response->status!=200){
          if(response->status==401)
            throw std::runtime_error(
              "Authentication failed: Invalid authorization code");
          throw std::runtime_error("Token exchange failed ("+
            std::to_string(response->status)+"): "+response->statusText);
        }
        output.resolve(response->data);
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
}
